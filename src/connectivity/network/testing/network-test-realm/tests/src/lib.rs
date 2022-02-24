// Copyright 2021 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#![cfg(test)]

use std::borrow::Cow;

use anyhow::Result;
use component_events::events::Event as _;
use derivative::Derivative;
use fidl_fuchsia_component as fcomponent;
use fidl_fuchsia_io as fio;
use fidl_fuchsia_net as fnet;
use fidl_fuchsia_net_debug as fnet_debug;
use fidl_fuchsia_net_ext as fnet_ext;
use fidl_fuchsia_net_interfaces as fnet_interfaces;
use fidl_fuchsia_net_interfaces_ext as fnet_interfaces_ext;
use fidl_fuchsia_net_stack as fstack;
use fidl_fuchsia_net_test_realm as fntr;
use fuchsia_zircon as zx;
use futures::StreamExt as _;
use net_declare::{fidl_ip_v4, fidl_ip_v6, fidl_mac, fidl_subnet};
use netemul::Endpoint as _;
use netstack_testing_common::realms::{KnownServiceProvider, Netstack2, TestSandboxExt as _};
use packet::ParsablePacket as _;
use test_case::test_case;

const ETH1_MAC_ADDRESS: fnet::MacAddress = fidl_mac!("02:03:04:05:06:07");
const ETH2_MAC_ADDRESS: fnet::MacAddress = fidl_mac!("05:06:07:08:09:10");
const ETH1_INTERFACE_NAME: &'static str = "eth1";
const ETH2_INTERFACE_NAME: &'static str = "eth2";
const EXPECTED_INTERFACE_NAME: &'static str = "added-interface";
const FAKE_STUB_URL: &'static str = "#meta/test-stub.cm";
const TEST_STUB_MONIKER_REGEX: &'static str = ".*/stubs:test-stub";

const DEFAULT_IPV4_TARGET_SUBNET: fnet::Subnet = fidl_subnet!("192.168.255.1/16");
const DEFAULT_IPV6_TARGET_SUBNET: fnet::Subnet = fidl_subnet!("3080::2/64");
const DEFAULT_IPV6_LINK_LOCAL_TARGET_SUBNET: fnet::Subnet = fidl_subnet!("fe80::1/64");
const DEFAULT_IPV4_SOURCE_SUBNET: fnet::Subnet = fidl_subnet!("192.168.254.1/16");
const DEFAULT_IPV6_SOURCE_SUBNET: fnet::Subnet = fidl_subnet!("3080::1/64");
const DEFAULT_IPV6_LINK_LOCAL_SOURCE_SUBNET: fnet::Subnet = fidl_subnet!("fe80::2/64");

const DURATION_FIVE_MINUTES: zx::Duration = zx::Duration::from_minutes(5);
const MINIMUM_TIMEOUT: zx::Duration = zx::Duration::from_nanos(1);
const NO_WAIT_TIMEOUT: zx::Duration = zx::Duration::from_nanos(0);
const DEFAULT_PAYLOAD_LENGTH: u16 = 100;
const NON_EXISTENT_INTERFACE_NAME: &'static str = "non_existent_interface";

const DEFAULT_IPV4_MULTICAST_ADDRESS: fnet::Ipv4Address = fidl_ip_v4!("224.1.2.3");
const DEFAULT_IPV6_MULTICAST_ADDRESS: fnet::Ipv6Address = fidl_ip_v6!("ff02::3");
const SOLICITED_NODE_MULTICAST_ADDRESS_PREFIX: net_types::ip::Subnet<net_types::ip::Ipv6Addr> = unsafe {
    net_types::ip::Subnet::new_unchecked(
        net_types::ip::Ipv6Addr::new([0xff02, 0, 0, 0, 0, 0x0001, 0xff00, 0]),
        104,
    )
};
const DEFAULT_INTERFACE_ID: u64 = 77;

/// Creates a `netemul::TestRealm` with a Netstack2 instance and the Network
/// Test Realm.
fn create_netstack_realm<'a>(
    name: impl Into<Cow<'a, str>>,
    sandbox: &'a netemul::TestSandbox,
) -> Result<netemul::TestRealm<'a>> {
    // NOTE: To simplify the tests and reduce the number of dependencies, netcfg
    // is intentionally omitted from the `KnownServiceProvider` list below.
    // Instead, it is expected that tests will manually register interfaces with
    // the system's Netstack as needed.
    sandbox.create_netstack_realm_with::<Netstack2, _, _>(
        name,
        &[KnownServiceProvider::NetworkTestRealm],
    )
}

/// Verifies that an interface with `interface_name` exists and has the provided
/// `expected_online_status`.
///
/// Note that this function will not return until the `expected_online_status`
/// is observed.
async fn wait_interface_online_status<'a>(
    interface_name: &'a str,
    expected_online_status: bool,
    state_proxy: &'a fnet_interfaces::StateProxy,
) {
    let id = get_interface_id(interface_name, state_proxy).await.unwrap_or_else(|| {
        panic!("failed to find interface with name {}", interface_name);
    });
    let () = fnet_interfaces_ext::wait_interface_with_id(
        fnet_interfaces_ext::event_stream_from_state(state_proxy).expect("watcher creation failed"),
        &mut fnet_interfaces_ext::InterfaceState::Unknown(id),
        |&fnet_interfaces_ext::Properties { online, .. }| {
            (expected_online_status == online).then(|| ())
        },
    )
    .await
    .expect("wait for interface failed");
}

/// Verifies that an interface with `interface_name` does not exist.
async fn verify_interface_not_exist<'a>(
    interface_name: &'a str,
    state_proxy: &'a fnet_interfaces::StateProxy,
) {
    assert_eq!(get_interface_id(interface_name, state_proxy).await, None);
}

/// Returns the id for the interface with `interface_name`.
///
/// If the interface is not found then, None is returned.
async fn get_interface_id<'a>(
    interface_name: &'a str,
    state_proxy: &'a fnet_interfaces::StateProxy,
) -> Option<u64> {
    network_test_realm::get_interface_id(interface_name, state_proxy)
        .await
        .expect("failed to obtain interface id")
}

/// Returns the id of the hermetic Netstack interface with `interface_name`.
///
/// Panics if the interface could not be found.
async fn expect_hermetic_interface_id<'a>(
    interface_name: &'a str,
    realm: &netemul::TestRealm<'a>,
) -> u64 {
    let state_proxy =
        connect_to_hermetic_network_realm_protocol::<fnet_interfaces::StateMarker>(realm).await;
    get_interface_id(interface_name, &state_proxy).await.unwrap_or_else(|| {
        panic!("failed to find hermetic Netstack interface with name {}", interface_name);
    })
}

/// Connects to a protocol within the hermetic network realm.
async fn connect_to_hermetic_network_realm_protocol<
    P: fidl::endpoints::DiscoverableProtocolMarker,
>(
    realm: &netemul::TestRealm<'_>,
) -> P::Proxy {
    let directory_proxy = open_hermetic_network_realm_exposed_directory(realm).await;
    fuchsia_component::client::connect_to_protocol_at_dir_root::<P>(&directory_proxy)
        .unwrap_or_else(|e| {
            panic!(
                "failed to connect to hermetic network realm protocol {} with error: {:?}",
                P::NAME,
                e
            )
        })
}

/// Opens the exposed directory that corresponds to the hermetic network realm.
///
/// An error will be returned if the realm does not exist.
async fn open_hermetic_network_realm_exposed_directory(
    realm: &netemul::TestRealm<'_>,
) -> fio::DirectoryProxy {
    let realm_proxy = realm
        .connect_to_protocol::<fcomponent::RealmMarker>()
        .expect("failed to connect to realm protocol");
    let (directory_proxy, server_end) = fidl::endpoints::create_proxy::<fio::DirectoryMarker>()
        .expect("failed to create Directory proxy");
    let mut child_ref = network_test_realm::create_hermetic_network_realm_child_ref();
    realm_proxy
        .open_exposed_dir(&mut child_ref, server_end)
        .await
        .expect("open_exposed_dir failed")
        .expect("open_exposed_dir error");
    directory_proxy
}

/// Returns true if the hermetic network realm exists.
async fn has_hermetic_network_realm(realm: &netemul::TestRealm<'_>) -> bool {
    let realm_proxy = realm
        .connect_to_protocol::<fcomponent::RealmMarker>()
        .expect("failed to connect to realm protocol");
    network_test_realm::has_hermetic_network_realm(&realm_proxy)
        .await
        .expect("failed to check for hermetic network realm")
}

async fn has_stub(realm: &netemul::TestRealm<'_>) -> bool {
    let realm_proxy =
        connect_to_hermetic_network_realm_protocol::<fcomponent::RealmMarker>(realm).await;
    network_test_realm::has_stub(&realm_proxy).await.expect("failed to check for stub")
}

async fn add_interface_to_system_netstack<'a>(
    mac_address: fnet::MacAddress,
    name: &'a str,
    sandbox: &'a netemul::TestSandbox,
    realm: &'a netemul::TestRealm<'a>,
) -> netemul::TestInterface<'a> {
    let endpoint = sandbox
        .create_endpoint_with(
            name,
            netemul::Ethernet::make_config(netemul::DEFAULT_MTU, Some(mac_address)),
        )
        .await
        .expect("failed to create endpoint");
    realm
        .install_endpoint(endpoint, &netemul::InterfaceConfig::None, Some(name.to_string()))
        .await
        .expect("failed to install endpoint")
}

async fn add_interface_to_devfs<'a>(
    name: &'a str,
    endpoint: &'a netemul::TestEndpoint<'a>,
    realm: &'a netemul::TestRealm<'a>,
) {
    let endpoint_mount_path = netemul::Ethernet::dev_path(name);
    let endpoint_mount_path = endpoint_mount_path.as_path();
    realm
        .add_virtual_device(endpoint, endpoint_mount_path)
        .await
        .expect("failed to add interface to devfs");
}

/// Adds an enabled interface with `mac_address` and `name` to the provided
/// `realm`.
async fn add_enabled_interface_to_realm<'a>(
    mac_address: fnet::MacAddress,
    name: &'a str,
    sandbox: &'a netemul::TestSandbox,
    realm: &'a netemul::TestRealm<'a>,
) -> netemul::TestInterface<'a> {
    let interface = add_interface_to_system_netstack(mac_address, name, sandbox, realm).await;
    add_interface_to_devfs(name, interface.endpoint(), realm).await;
    interface
}

/// Adds the address from the specified `subnet` to the hermetic Netstack
/// interface that has the provided `interface_name`.
///
/// A forwarding entry is also added for the relevant interface and the provided
/// `subnet`.
async fn add_address_to_hermetic_interface(
    interface_name: &str,
    subnet: fnet::Subnet,
    realm: &netemul::TestRealm<'_>,
) {
    let state_proxy =
        connect_to_hermetic_network_realm_protocol::<fnet_interfaces::StateMarker>(realm).await;
    let id = get_interface_id(interface_name, &state_proxy).await.unwrap_or_else(|| {
        panic!("failed to find interface with name {}", interface_name);
    });
    let interfaces_proxy =
        connect_to_hermetic_network_realm_protocol::<fnet_debug::InterfacesMarker>(realm).await;
    let (control, server_end) =
        fnet_interfaces_ext::admin::Control::create_endpoints().expect("create_endpoints failed");
    interfaces_proxy.get_admin(id, server_end).expect("get_admin failed");

    let fnet::Subnet { addr, prefix_len } = &subnet;
    let interface_address = match addr {
        fidl_fuchsia_net::IpAddress::Ipv4(ipv4_addr) => {
            fidl_fuchsia_net::InterfaceAddress::Ipv4(fidl_fuchsia_net::Ipv4AddressWithPrefix {
                addr: ipv4_addr.clone(),
                prefix_len: *prefix_len,
            })
        }
        fidl_fuchsia_net::IpAddress::Ipv6(ipv6_addr) => {
            fidl_fuchsia_net::InterfaceAddress::Ipv6(ipv6_addr.clone())
        }
    };

    let address_state_provider = netstack_testing_common::interfaces::add_address_wait_assigned(
        &control,
        interface_address,
        fidl_fuchsia_net_interfaces_admin::AddressParameters::EMPTY,
    )
    .await
    .expect("add_address_wait_assigned failed");

    // Allow the address to live beyond the `address_state_provider` handle.
    address_state_provider.detach().expect("detatch failed");

    // Subnet forwarding entries are not automatically configured when an
    // address is added using the `Control` protocol.
    let stack_proxy =
        connect_to_hermetic_network_realm_protocol::<fstack::StackMarker>(&realm).await;
    stack_proxy
        .add_forwarding_entry(&mut fidl_fuchsia_net_stack::ForwardingEntry {
            subnet: fnet_ext::apply_subnet_mask(subnet),
            device_id: id,
            next_hop: None,
            metric: 0,
        })
        .await
        .expect("add_forwarding_entry failed")
        .expect("add_forwarding_entry error");
}

/// Adds an interface to the hermetic Netstack with `interface_name` and
/// `mac_address`.
///
/// The added interface is assigned a static IP address based on `subnet`.
/// Additionally, the interface joins the provided `network`.
async fn join_network_with_hermetic_netstack<'a>(
    realm: &'a netemul::TestRealm<'a>,
    network: &'a netemul::TestNetwork<'a>,
    network_test_realm: &'a fntr::ControllerProxy,
    interface_name: &'a str,
    mac_address: fnet::MacAddress,
    subnet: fnet::Subnet,
) -> netemul::TestInterface<'a> {
    let interface = realm
        .join_network_with(
            &network,
            interface_name,
            netemul::Ethernet::make_config(netemul::DEFAULT_MTU, Some(mac_address)),
            &netemul::InterfaceConfig::None,
        )
        .await
        .expect("join_network failed");
    add_interface_to_devfs(interface_name, interface.endpoint(), &realm).await;

    network_test_realm
        .add_interface(&mut mac_address.clone(), interface_name)
        .await
        .expect("add_interface failed")
        .expect("add_interface error");

    add_address_to_hermetic_interface(interface_name, subnet, realm).await;
    interface
}

#[fuchsia_async::run_singlethreaded(test)]
async fn start_hermetic_network_realm() {
    let sandbox = netemul::TestSandbox::new().expect("failed to create sandbox");
    let realm = create_netstack_realm("start_hermetic_network_realm", &sandbox)
        .expect("failed to create netstack realm");

    let network_test_realm = realm
        .connect_to_protocol::<fntr::ControllerMarker>()
        .expect("failed to connect to network test realm controller");

    network_test_realm
        .start_hermetic_network_realm(fntr::Netstack::V2)
        .await
        .expect("start_hermetic_network_realm failed")
        .expect("start_hermetic_network_realm error");

    assert!(has_hermetic_network_realm(&realm).await);
}

#[fuchsia_async::run_singlethreaded(test)]
async fn start_hermetic_network_realm_replaces_existing_realm() {
    let sandbox = netemul::TestSandbox::new().expect("failed to create sandbox");
    let realm =
        create_netstack_realm("start_hermetic_network_realm_replaces_existing_realm", &sandbox)
            .expect("failed to create netstack realm");

    let network_test_realm = realm
        .connect_to_protocol::<fntr::ControllerMarker>()
        .expect("failed to connect to network test realm controller");

    network_test_realm
        .start_hermetic_network_realm(fntr::Netstack::V2)
        .await
        .expect("start_hermetic_network_realm failed")
        .expect("start_hermetic_network_realm error");

    let _interface: netemul::TestInterface<'_> =
        add_enabled_interface_to_realm(ETH1_MAC_ADDRESS, ETH1_INTERFACE_NAME, &sandbox, &realm)
            .await;

    network_test_realm
        .add_interface(&mut ETH1_MAC_ADDRESS.clone(), EXPECTED_INTERFACE_NAME)
        .await
        .expect("add_interface failed")
        .expect("add_interface error");

    network_test_realm
        .start_hermetic_network_realm(fntr::Netstack::V2)
        .await
        .expect("start_hermetic_network_realm failed")
        .expect("start_hermetic_network_realm error");

    let system_state_proxy = realm
        .connect_to_protocol::<fnet_interfaces::StateMarker>()
        .expect("failed to connect to state");

    // The interface on the system's Netstack should be re-enabled (it was
    // disabled when an interface was added above).
    wait_interface_online_status(
        ETH1_INTERFACE_NAME,
        true, /* expected_online_status */
        &system_state_proxy,
    )
    .await;

    let hermetic_network_state_proxy =
        connect_to_hermetic_network_realm_protocol::<fnet_interfaces::StateMarker>(&realm).await;

    // The Netstack in the replaced hermetic network realm should not have the
    // previously attached interface.
    verify_interface_not_exist(EXPECTED_INTERFACE_NAME, &hermetic_network_state_proxy).await;

    assert!(has_hermetic_network_realm(&realm).await);
}

#[fuchsia_async::run_singlethreaded(test)]
async fn add_interface() {
    let sandbox = netemul::TestSandbox::new().expect("failed to create sandbox");
    let realm =
        create_netstack_realm("add_interface", &sandbox).expect("failed to create netstack realm");

    let network_test_realm = realm
        .connect_to_protocol::<fntr::ControllerMarker>()
        .expect("failed to connect to network test realm controller");

    network_test_realm
        .start_hermetic_network_realm(fntr::Netstack::V2)
        .await
        .expect("start_hermetic_network_realm failed")
        .expect("start_hermetic_network_realm error");

    let _interface: netemul::TestInterface<'_> =
        add_enabled_interface_to_realm(ETH1_MAC_ADDRESS, ETH1_INTERFACE_NAME, &sandbox, &realm)
            .await;

    let _interface: netemul::TestInterface<'_> =
        add_enabled_interface_to_realm(ETH2_MAC_ADDRESS, ETH2_INTERFACE_NAME, &sandbox, &realm)
            .await;

    network_test_realm
        .add_interface(&mut ETH1_MAC_ADDRESS.clone(), EXPECTED_INTERFACE_NAME)
        .await
        .expect("add_interface failed")
        .expect("add_interface error");

    let system_state_proxy = realm
        .connect_to_protocol::<fnet_interfaces::StateMarker>()
        .expect("failed to connect to state");

    // The corresponding interface on the system's Netstack should be disabled
    // when an interface is added to the hermetic Netstack.
    wait_interface_online_status(
        ETH1_INTERFACE_NAME,
        false, /* expected_online_status */
        &system_state_proxy,
    )
    .await;

    let hermetic_network_state_proxy =
        connect_to_hermetic_network_realm_protocol::<fnet_interfaces::StateMarker>(&realm).await;

    // An interface with a name of `EXPECTED_INTERFACE_NAME` should be enabled and
    // present in the hermetic Netstack.
    wait_interface_online_status(
        EXPECTED_INTERFACE_NAME,
        true, /* expected_online_status */
        &hermetic_network_state_proxy,
    )
    .await;
}

// Tests the case where the MAC address provided to `Controller.AddInterface`
// does not match any of the interfaces on the system.
#[fuchsia_async::run_singlethreaded(test)]
async fn add_interface_with_no_matching_interface() {
    let sandbox = netemul::TestSandbox::new().expect("failed to create sandbox");
    let realm = create_netstack_realm("add_interface_with_no_matching_interface", &sandbox)
        .expect("failed to create netstack realm");

    let network_test_realm = realm
        .connect_to_protocol::<fntr::ControllerMarker>()
        .expect("failed to connect to network test realm controller");

    network_test_realm
        .start_hermetic_network_realm(fntr::Netstack::V2)
        .await
        .expect("start_hermetic_network_realm failed")
        .expect("start_hermetic_network_realm error");

    let _interface: netemul::TestInterface<'_> =
        add_enabled_interface_to_realm(ETH1_MAC_ADDRESS, ETH1_INTERFACE_NAME, &sandbox, &realm)
            .await;

    // `non_matching_mac_address` doesn't match any of the MAC addresses for
    // interfaces owned by the system's Netstack.
    let mut non_matching_mac_address = fidl_mac!("aa:bb:cc:dd:ee:ff");
    assert_eq!(
        network_test_realm
            .add_interface(&mut non_matching_mac_address, EXPECTED_INTERFACE_NAME)
            .await
            .expect("failed to add interface to hermetic netstack"),
        Err(fntr::Error::InterfaceNotFound)
    );
}

// Tests the case where the MAC address provided to `Controller.AddInterface`
// matches an interface on the system Netstack, but not in devfs.
#[fuchsia_async::run_singlethreaded(test)]
async fn add_interface_with_no_matching_interface_in_devfs() {
    let sandbox = netemul::TestSandbox::new().expect("failed to create sandbox");
    let realm =
        create_netstack_realm("add_interface_with_no_matching_interface_in_devfs", &sandbox)
            .expect("failed to create netstack realm");

    let network_test_realm = realm
        .connect_to_protocol::<fntr::ControllerMarker>()
        .expect("failed to connect to network test realm controller");

    network_test_realm
        .start_hermetic_network_realm(fntr::Netstack::V2)
        .await
        .expect("start_hermetic_network_realm failed")
        .expect("start_hermetic_network_realm error");

    let _: netemul::TestInterface<'_> =
        add_interface_to_system_netstack(ETH1_MAC_ADDRESS, ETH1_INTERFACE_NAME, &sandbox, &realm)
            .await;

    // The Network Test Realm requires that the matching interface be present in
    // both the system's Netstack and devfs. In this case, it is only present in
    // the system's Netstack.
    assert_eq!(
        network_test_realm
            .add_interface(&mut ETH1_MAC_ADDRESS.clone(), EXPECTED_INTERFACE_NAME)
            .await
            .expect("failed to add interface to hermetic netstack"),
        Err(fntr::Error::InterfaceNotFound)
    );
}

// Tests the case where the MAC address provided to `Controller.AddInterface`
// matches an interface in devfs, but not in the system Netstack.
#[fuchsia_async::run_singlethreaded(test)]
async fn add_interface_with_no_matching_interface_in_netstack() {
    let sandbox = netemul::TestSandbox::new().expect("failed to create sandbox");
    let realm =
        create_netstack_realm("add_interface_with_no_matching_interface_in_netstack", &sandbox)
            .expect("failed to create netstack realm");

    let network_test_realm = realm
        .connect_to_protocol::<fntr::ControllerMarker>()
        .expect("failed to connect to network test realm controller");

    network_test_realm
        .start_hermetic_network_realm(fntr::Netstack::V2)
        .await
        .expect("start_hermetic_network_realm failed")
        .expect("start_hermetic_network_realm error");

    let endpoint = sandbox
        .create_endpoint_with(
            ETH1_INTERFACE_NAME,
            netemul::Ethernet::make_config(netemul::DEFAULT_MTU, Some(ETH1_MAC_ADDRESS)),
        )
        .await
        .expect("failed to create endpoint");
    add_interface_to_devfs(ETH1_INTERFACE_NAME, &endpoint, &realm).await;

    // The Network Test Realm requires that the matching interface be present in
    // both the system's Netstack and devfs. In this case, it is only present in
    // devfs.
    assert_eq!(
        network_test_realm
            .add_interface(&mut ETH1_MAC_ADDRESS.clone(), EXPECTED_INTERFACE_NAME)
            .await
            .expect("failed to add interface to hermetic netstack"),
        Err(fntr::Error::InterfaceNotFound)
    );
}

#[fuchsia_async::run_singlethreaded(test)]
async fn stop_hermetic_network_realm() {
    let sandbox = netemul::TestSandbox::new().expect("failed to create sandbox");
    let realm = create_netstack_realm("stop_hermetic_network_realm", &sandbox)
        .expect("failed to create netstack realm");

    let network_test_realm = realm
        .connect_to_protocol::<fntr::ControllerMarker>()
        .expect("failed to connect to network test realm controller");

    network_test_realm
        .start_hermetic_network_realm(fntr::Netstack::V2)
        .await
        .expect("start_hermetic_network_realm failed")
        .expect("start_hermetic_network_realm error");

    let _interface: netemul::TestInterface<'_> =
        add_enabled_interface_to_realm(ETH1_MAC_ADDRESS, ETH1_INTERFACE_NAME, &sandbox, &realm)
            .await;

    network_test_realm
        .add_interface(&mut ETH1_MAC_ADDRESS.clone(), EXPECTED_INTERFACE_NAME)
        .await
        .expect("add_interface failed")
        .expect("add_interface error");

    network_test_realm
        .stop_hermetic_network_realm()
        .await
        .expect("stop_hermetic_network_realm failed")
        .expect("stop_hermetic_network_realm error");

    let system_state_proxy = realm
        .connect_to_protocol::<fnet_interfaces::StateMarker>()
        .expect("failed to connect to state");

    wait_interface_online_status(
        ETH1_INTERFACE_NAME,
        true, /* expected_online_status */
        &system_state_proxy,
    )
    .await;
    assert!(!has_hermetic_network_realm(&realm).await);
}

#[fuchsia_async::run_singlethreaded(test)]
async fn stop_hermetic_network_realm_with_no_existing_realm() {
    let sandbox = netemul::TestSandbox::new().expect("failed to create sandbox");
    let realm =
        create_netstack_realm("stop_hermetic_network_realm_with_no_existing_realm", &sandbox)
            .expect("failed to create netstack realm");

    let network_test_realm = realm
        .connect_to_protocol::<fntr::ControllerMarker>()
        .expect("failed to connect to network test realm controller");

    assert_eq!(
        network_test_realm
            .stop_hermetic_network_realm()
            .await
            .expect("failed to stop hermetic network realm"),
        Err(fntr::Error::HermeticNetworkRealmNotRunning),
    );
}

#[fuchsia_async::run_singlethreaded(test)]
async fn start_stub() {
    let sandbox = netemul::TestSandbox::new().expect("failed to create sandbox");
    let realm =
        create_netstack_realm("start_stub", &sandbox).expect("failed to create netstack realm");

    let network_test_realm = realm
        .connect_to_protocol::<fntr::ControllerMarker>()
        .expect("failed to connect to network test realm controller");

    network_test_realm
        .start_hermetic_network_realm(fntr::Netstack::V2)
        .await
        .expect("start_hermetic_network_realm failed")
        .expect("start_hermetic_network_realm error");

    network_test_realm
        .start_stub(FAKE_STUB_URL)
        .await
        .expect("start_stub failed")
        .expect("start_stub error");

    assert!(has_stub(&realm).await);
}

#[fuchsia_async::run_singlethreaded(test)]
async fn start_stub_with_existing_stub() {
    let sandbox = netemul::TestSandbox::new().expect("failed to create sandbox");
    let realm = create_netstack_realm("start_stub_with_existing_stub", &sandbox)
        .expect("failed to create netstack realm");

    let network_test_realm = realm
        .connect_to_protocol::<fntr::ControllerMarker>()
        .expect("failed to connect to network test realm controller");

    network_test_realm
        .start_hermetic_network_realm(fntr::Netstack::V2)
        .await
        .expect("start_hermetic_network_realm failed")
        .expect("start_hermetic_network_realm error");

    let event_source =
        component_events::events::EventSource::new().expect("failed to create event source");

    let mut event_stream = event_source
        .subscribe(vec![component_events::events::EventSubscription::new(
            vec![component_events::events::Started::NAME, component_events::events::Stopped::NAME],
            component_events::events::EventMode::Async,
        )])
        .await
        .expect("failed to subscribe to EventSource");

    network_test_realm
        .start_stub(FAKE_STUB_URL)
        .await
        .expect("start_stub failed")
        .expect("start_stub error");

    let event_matcher =
        component_events::matcher::EventMatcher::ok().moniker_regex(TEST_STUB_MONIKER_REGEX);

    let component_events::events::StartedPayload {} = event_matcher
        .clone()
        .wait::<component_events::events::Started>(&mut event_stream)
        .await
        .expect("initial test-stub observe start event failed")
        .result()
        .expect("initial test-stub observe start event error");

    network_test_realm
        .start_stub(FAKE_STUB_URL)
        .await
        .expect("start_stub replace failed")
        .expect("start_stub replace error");

    // Verify that the previously running stub was replaced. That is, check that
    // the stub was stopped and then started.
    let stopped_event = event_matcher
        .clone()
        .wait::<component_events::events::Stopped>(&mut event_stream)
        .await
        .expect("test-stub observe stop event failed");

    // Note that stopped_event.result below borrows from `stopped_event`. As a
    // result it needs to be in a different statement.
    let component_events::events::StoppedPayload { status } =
        stopped_event.result().expect("test-stub observe stop event error");
    assert_eq!(
        *status,
        component_events::events::ExitStatus::Crash(zx::Status::PEER_CLOSED.into_raw())
    );

    let component_events::events::StartedPayload {} = event_matcher
        .clone()
        .wait::<component_events::events::Started>(&mut event_stream)
        .await
        .expect("replacement test-stub observe start event failed")
        .result()
        .expect("replacement test-stub observe start event error");

    assert!(has_stub(&realm).await);
}

#[fuchsia_async::run_singlethreaded(test)]
async fn start_stub_with_non_existent_component() {
    let sandbox = netemul::TestSandbox::new().expect("failed to create sandbox");
    let realm = create_netstack_realm("start_stub_with_non_existent_component", &sandbox)
        .expect("failed to create netstack realm");

    let network_test_realm = realm
        .connect_to_protocol::<fntr::ControllerMarker>()
        .expect("failed to connect to network test realm controller");

    network_test_realm
        .start_hermetic_network_realm(fntr::Netstack::V2)
        .await
        .expect("start_hermetic_network_realm failed")
        .expect("start_hermetic_network_realm error");

    assert_eq!(
        network_test_realm
            .start_stub("#meta/non-existent-stub.cm")
            .await
            .expect("failed to call start_stub"),
        Err(fntr::Error::ComponentNotFound),
    );
}

#[fuchsia_async::run_singlethreaded(test)]
async fn start_stub_with_malformed_component_url() {
    let sandbox = netemul::TestSandbox::new().expect("failed to create sandbox");
    let realm = create_netstack_realm("start_stub_with_malformed_component_url", &sandbox)
        .expect("failed to create netstack realm");

    let network_test_realm = realm
        .connect_to_protocol::<fntr::ControllerMarker>()
        .expect("failed to connect to network test realm controller");

    network_test_realm
        .start_hermetic_network_realm(fntr::Netstack::V2)
        .await
        .expect("start_hermetic_network_realm failed")
        .expect("start_hermetic_network_realm error");

    assert_eq!(
        network_test_realm
            .start_stub("malformed-component-url")
            .await
            .expect("failed to call start_stub"),
        Err(fntr::Error::InvalidArguments),
    );
}

#[fuchsia_async::run_singlethreaded(test)]
async fn start_stub_with_no_hermetic_network_realm() {
    let sandbox = netemul::TestSandbox::new().expect("failed to create sandbox");
    let realm = create_netstack_realm("start_stub_with_no_hermetic_network_realm", &sandbox)
        .expect("failed to create netstack realm");

    let network_test_realm = realm
        .connect_to_protocol::<fntr::ControllerMarker>()
        .expect("failed to connect to network test realm controller");

    assert_eq!(
        network_test_realm.start_stub(FAKE_STUB_URL).await.expect("failed to call start_stub"),
        Err(fntr::Error::HermeticNetworkRealmNotRunning),
    );
}

#[fuchsia_async::run_singlethreaded(test)]
async fn stop_stub() {
    let sandbox = netemul::TestSandbox::new().expect("failed to create sandbox");
    let realm =
        create_netstack_realm("stop_stub", &sandbox).expect("failed to create netstack realm");

    let network_test_realm = realm
        .connect_to_protocol::<fntr::ControllerMarker>()
        .expect("failed to connect to network test realm controller");

    network_test_realm
        .start_hermetic_network_realm(fntr::Netstack::V2)
        .await
        .expect("start_hermetic_network_realm failed")
        .expect("start_hermetic_network_realm error");

    network_test_realm
        .start_stub(FAKE_STUB_URL)
        .await
        .expect("start_stub failed")
        .expect("start_stub error");

    network_test_realm.stop_stub().await.expect("stop_stub failed").expect("stop_stub error");

    assert!(!has_stub(&realm).await);
}

#[fuchsia_async::run_singlethreaded(test)]
async fn stop_stub_with_no_running_stub() {
    let sandbox = netemul::TestSandbox::new().expect("failed to create sandbox");
    let realm = create_netstack_realm("stop_stub_with_no_running_stub", &sandbox)
        .expect("failed to create netstack realm");

    let network_test_realm = realm
        .connect_to_protocol::<fntr::ControllerMarker>()
        .expect("failed to connect to network test realm controller");

    network_test_realm
        .start_hermetic_network_realm(fntr::Netstack::V2)
        .await
        .expect("start_hermetic_network_realm failed")
        .expect("start_hermetic_network_realm error");

    assert_eq!(
        network_test_realm.stop_stub().await.expect("failed to call stop_stub"),
        Err(fntr::Error::StubNotRunning),
    );
}

#[fuchsia_async::run_singlethreaded(test)]
async fn stop_stub_with_no_hermetic_network_realm() {
    let sandbox = netemul::TestSandbox::new().expect("failed to create sandbox");
    let realm = create_netstack_realm("stop_stub_with_no_hermetic_network_realm", &sandbox)
        .expect("failed to create netstack realm");

    let network_test_realm = realm
        .connect_to_protocol::<fntr::ControllerMarker>()
        .expect("failed to connect to network test realm controller");

    assert_eq!(
        network_test_realm.stop_stub().await.expect("failed to call stop_stub"),
        Err(fntr::Error::HermeticNetworkRealmNotRunning),
    );
}

/// Defaultable configuration options for ping tests.
#[derive(Debug, Derivative)]
#[derivative(Default)]
struct PingOptions {
    interface_name: Option<String>,
    #[derivative(Default(value = "DEFAULT_PAYLOAD_LENGTH"))]
    payload_length: u16,
    #[derivative(Default(value = "DURATION_FIVE_MINUTES"))]
    timeout: zx::Duration,
    disable_target_interface: bool,
}

/// Address configuration for ping tests.
struct PingAddressConfig {
    source_subnet: fnet::Subnet,
    target_subnet: fnet::Subnet,
}

const IPV4_ADDRESS_CONFIG: PingAddressConfig = PingAddressConfig {
    source_subnet: DEFAULT_IPV4_SOURCE_SUBNET,
    target_subnet: DEFAULT_IPV4_TARGET_SUBNET,
};
const IPV6_ADDRESS_CONFIG: PingAddressConfig = PingAddressConfig {
    source_subnet: DEFAULT_IPV6_SOURCE_SUBNET,
    target_subnet: DEFAULT_IPV6_TARGET_SUBNET,
};
const IPV6_LINK_LOCAL_ADDRESS_CONFIG: PingAddressConfig = PingAddressConfig {
    source_subnet: DEFAULT_IPV6_LINK_LOCAL_SOURCE_SUBNET,
    target_subnet: DEFAULT_IPV6_LINK_LOCAL_TARGET_SUBNET,
};

#[test_case(
    "ipv4",
    IPV4_ADDRESS_CONFIG,
    PingOptions::default(),
    Ok(());
    "ipv4")]
#[test_case(
    "ipv4_bind_to_existing_interface",
    IPV4_ADDRESS_CONFIG,
    PingOptions {
        interface_name:  Some(ETH1_INTERFACE_NAME.to_string()),
        ..PingOptions::default()
    },
    Ok(());
    "ipv4 bind to existing interface")]
#[test_case(
    "ipv4_bind_to_non_existent_interface",
    IPV4_ADDRESS_CONFIG,
    PingOptions {
        interface_name: Some(NON_EXISTENT_INTERFACE_NAME.to_string()),
        ..PingOptions::default()
    },
    Err(fntr::Error::InterfaceNotFound);
    "ipv4 bind to non existent interface")]
#[test_case(
    "ipv6",
    IPV6_ADDRESS_CONFIG,
    PingOptions::default(),
    Ok(());
    "ipv6")]
#[test_case(
    "ipv6_bind_to_existing_interface",
    IPV6_ADDRESS_CONFIG,
    PingOptions {
        interface_name:  Some(ETH1_INTERFACE_NAME.to_string()),
        ..PingOptions::default()
    },
    Ok(());
    "ipv6 bind to existing interface")]
#[test_case(
    "ipv6_link_local_bind_to_existing_interface",
    IPV6_LINK_LOCAL_ADDRESS_CONFIG,
    PingOptions {
        interface_name:  Some(ETH1_INTERFACE_NAME.to_string()),
        ..PingOptions::default()
    },
    Ok(());
    "ipv6 link local bind to existing interface")]
#[test_case(
    "ipv6_link_local_with_no_interface_specified",
    IPV6_LINK_LOCAL_ADDRESS_CONFIG,
    PingOptions::default(),
    Err(fntr::Error::InvalidArguments);
    "ipv6 link local with no interface specified")]
#[test_case(
    "ipv6_bind_to_non_existent_interface",
    IPV6_ADDRESS_CONFIG,
    PingOptions {
        interface_name: Some(NON_EXISTENT_INTERFACE_NAME.to_string()),
        ..PingOptions::default()
    },
    Err(fntr::Error::InterfaceNotFound);
    "ipv6 bind to non existent interface")]
#[test_case(
    "timeout_exceeded",
    IPV4_ADDRESS_CONFIG,
    // Attempting to ping a target interface that is disabled forces a timeout.
    PingOptions {
        disable_target_interface: true,
        timeout: MINIMUM_TIMEOUT, ..PingOptions::default()
    },
    Err(fntr::Error::TimeoutExceeded);
    "timeout exceeded")]
#[test_case(
    "no_timeout_with_disabled_target_interface",
    IPV4_ADDRESS_CONFIG,
    PingOptions {
        disable_target_interface: true,
        timeout: NO_WAIT_TIMEOUT,
        ..PingOptions::default()
    },
    // Since no timeout is defined, this ping should succeed.
    Ok(());
    "no timeout with disabled target interface")]
#[test_case(
    "no_timeout",
    IPV4_ADDRESS_CONFIG,
    PingOptions { timeout: NO_WAIT_TIMEOUT, ..PingOptions::default() },
    Ok(());
    "no timeout")]
#[test_case(
    "host_unreachable",
    PingAddressConfig {
        target_subnet: fidl_subnet!("192.167.1.1/16"),
        ..IPV4_ADDRESS_CONFIG
    },
    PingOptions {
        interface_name:  Some(ETH1_INTERFACE_NAME.to_string()),
        ..PingOptions::default()
    },
    Err(fntr::Error::PingFailed);
    "host unreachable")]
#[test_case(
    "oversized_payload_length",
    IPV4_ADDRESS_CONFIG,
    PingOptions { payload_length: u16::MAX, ..PingOptions::default() },
    Err(fntr::Error::InvalidArguments);
    "oversized payload length")]
#[fuchsia_async::run_singlethreaded(test)]
async fn ping(
    name: &str,
    PingAddressConfig { source_subnet, mut target_subnet }: PingAddressConfig,
    PingOptions { interface_name, payload_length, timeout, disable_target_interface }: PingOptions,
    expected_result: Result<(), fntr::Error>,
) {
    let sandbox = netemul::TestSandbox::new().expect("failed to create sandbox");
    // Create a realm that contains a system Netstack and the Network Test
    // Realm.
    let realm = create_netstack_realm(format!("ping_{}", name), &sandbox)
        .expect("failed to create netstack realm");
    let network = sandbox.create_network("network").await.expect("failed to create network");

    // Create another Netstack realm that will be pinged by the hermetic
    // Netstack.
    let target_realm = sandbox
        .create_netstack_realm::<Netstack2, _>(format!("ping_{}_target", name))
        .expect("failed to create target netstack realm");

    let target_ep = target_realm
        .join_network::<netemul::Ethernet, _>(
            &network,
            ETH2_INTERFACE_NAME,
            &netemul::InterfaceConfig::StaticIp(target_subnet),
        )
        .await
        .expect("join_network failed for target_realm");

    if disable_target_interface {
        // Disable the target interface and wait for it to achieve the disabled
        // state.
        let did_disable =
            target_ep.control().disable().await.expect("send disable").expect("disable interface");
        assert!(did_disable);
        let state_proxy = target_realm
            .connect_to_protocol::<fnet_interfaces::StateMarker>()
            .expect("failed to connect to state");
        wait_interface_online_status(
            ETH2_INTERFACE_NAME,
            false, /* expected_online_status */
            &state_proxy,
        )
        .await;
    }

    let system_ep = realm
        .join_network_with(
            &network,
            ETH1_INTERFACE_NAME,
            netemul::Ethernet::make_config(netemul::DEFAULT_MTU, Some(ETH1_MAC_ADDRESS)),
            &netemul::InterfaceConfig::None,
        )
        .await
        .expect("join_network failed for base realm");

    add_interface_to_devfs(ETH1_INTERFACE_NAME, system_ep.endpoint(), &realm).await;

    let network_test_realm = realm
        .connect_to_protocol::<fntr::ControllerMarker>()
        .expect("failed to connect to network test realm controller");

    network_test_realm
        .start_hermetic_network_realm(fntr::Netstack::V2)
        .await
        .expect("start_hermetic_network_realm failed")
        .expect("start_hermetic_network_realm error");

    network_test_realm
        .add_interface(&mut ETH1_MAC_ADDRESS.clone(), ETH1_INTERFACE_NAME)
        .await
        .expect("add_interface failed")
        .expect("add_interface error");

    add_address_to_hermetic_interface(ETH1_INTERFACE_NAME, source_subnet, &realm).await;

    assert_eq!(
        network_test_realm
            .ping(
                &mut target_subnet.addr,
                payload_length,
                interface_name.as_deref(),
                timeout.into_nanos(),
            )
            .await
            .expect("ping failed"),
        expected_result
    );
}

#[fuchsia_async::run_singlethreaded(test)]
async fn ping_with_no_hermetic_network_realm() {
    let sandbox = netemul::TestSandbox::new().expect("failed to create sandbox");
    let realm = create_netstack_realm("ping_with_no_hermetic_network_realm", &sandbox)
        .expect("failed to create netstack realm");

    let network_test_realm = realm
        .connect_to_protocol::<fntr::ControllerMarker>()
        .expect("failed to connect to network test realm controller");

    let mut target_ip = DEFAULT_IPV4_TARGET_SUBNET.addr;
    assert_eq!(
        network_test_realm
            .ping(&mut target_ip, DEFAULT_PAYLOAD_LENGTH, None, NO_WAIT_TIMEOUT.into_nanos())
            .await
            .expect("ping failed"),
        Err(fntr::Error::HermeticNetworkRealmNotRunning),
    );
}

#[fuchsia_async::run_singlethreaded(test)]
async fn ping_with_no_added_interface() {
    let sandbox = netemul::TestSandbox::new().expect("failed to create sandbox");
    let realm = create_netstack_realm("ping_with_no_added_interface", &sandbox)
        .expect("failed to create netstack realm");

    let network_test_realm = realm
        .connect_to_protocol::<fntr::ControllerMarker>()
        .expect("failed to connect to network test realm controller");

    network_test_realm
        .start_hermetic_network_realm(fntr::Netstack::V2)
        .await
        .expect("start_hermetic_network_realm failed")
        .expect("start_hermetic_network_realm error");

    let mut target_ip = DEFAULT_IPV4_TARGET_SUBNET.addr;
    assert_eq!(
        network_test_realm
            .ping(&mut target_ip, DEFAULT_PAYLOAD_LENGTH, None, NO_WAIT_TIMEOUT.into_nanos())
            .await
            .expect("ping failed"),
        Err(fntr::Error::PingFailed),
    );
}

#[derive(Debug, PartialEq)]
enum MulticastEvent {
    Joined(fnet::IpAddress),
    Left(fnet::IpAddress),
}

/// Extracts Ipv4 `MulticastEvent`s from the provided `data`.
fn extract_v4_multicast_event(data: &[u8]) -> Option<MulticastEvent> {
    let (mut payload, _src_ip, _dst_ip, proto, _ttl) =
        packet_formats::testutil::parse_ip_packet::<net_types::ip::Ipv4>(&data)
            .expect("error parsing IPv4 packet");

    if proto != packet_formats::ip::Ipv4Proto::Igmp {
        // Ignore non-IGMP packets.
        return None;
    }

    let igmp_packet = packet_formats::igmp::messages::IgmpPacket::parse(&mut payload, ())
        .expect("failed to parse IGMP packet");

    match igmp_packet {
        packet_formats::igmp::messages::IgmpPacket::MembershipReportV2(message) => {
            Some(MulticastEvent::Joined(fnet::IpAddress::Ipv4(fnet::Ipv4Address {
                addr: message.group_addr().ipv4_bytes(),
            })))
        }
        packet_formats::igmp::messages::IgmpPacket::LeaveGroup(message) => {
            Some(MulticastEvent::Left(fnet::IpAddress::Ipv4(fnet::Ipv4Address {
                addr: message.group_addr().ipv4_bytes(),
            })))
        }
        packet_formats::igmp::messages::IgmpPacket::MembershipReportV1(_)
        | packet_formats::igmp::messages::IgmpPacket::MembershipReportV3(_)
        | packet_formats::igmp::messages::IgmpPacket::MembershipQueryV2(_)
        | packet_formats::igmp::messages::IgmpPacket::MembershipQueryV3(_) => {
            panic!("unexpected IgmpPacket format: {:?}", igmp_packet)
        }
    }
}

/// Extracts Ipv6 `MulticastEvent`s from the provided `data`.
fn extract_v6_multicast_event(data: &[u8]) -> Option<MulticastEvent> {
    let (mut payload, src_ip, dst_ip, proto, _ttl) =
        packet_formats::testutil::parse_ip_packet::<net_types::ip::Ipv6>(&data)
            .expect("error parsing IPv6 packet");

    if proto != packet_formats::ip::Ipv6Proto::Icmpv6 {
        // Ignore non-ICMPv6 packets.
        return None;
    }

    let icmp_packet = packet_formats::icmp::Icmpv6Packet::parse(
        &mut payload,
        packet_formats::icmp::IcmpParseArgs::new(src_ip, dst_ip),
    )
    .expect("error parsing ICMPv6 packet");

    let mld_packet = match icmp_packet {
        packet_formats::icmp::Icmpv6Packet::Mld(mld) => mld,
        packet_formats::icmp::Icmpv6Packet::DestUnreachable(_)
        | packet_formats::icmp::Icmpv6Packet::EchoReply(_)
        | packet_formats::icmp::Icmpv6Packet::EchoRequest(_)
        | packet_formats::icmp::Icmpv6Packet::Ndp(_)
        | packet_formats::icmp::Icmpv6Packet::PacketTooBig(_)
        | packet_formats::icmp::Icmpv6Packet::ParameterProblem(_)
        | packet_formats::icmp::Icmpv6Packet::TimeExceeded(_) => return None,
    };

    match mld_packet {
        packet_formats::icmp::mld::MldPacket::MulticastListenerReport(packet) => {
            (!SOLICITED_NODE_MULTICAST_ADDRESS_PREFIX.contains(&packet.body().group_addr)).then(
                || {
                    MulticastEvent::Joined(fnet::IpAddress::Ipv6(fnet::Ipv6Address {
                        addr: packet.body().group_addr.ipv6_bytes(),
                    }))
                },
            )
        }
        packet_formats::icmp::mld::MldPacket::MulticastListenerDone(packet) => {
            (!SOLICITED_NODE_MULTICAST_ADDRESS_PREFIX.contains(&packet.body().group_addr)).then(
                || {
                    MulticastEvent::Left(fnet::IpAddress::Ipv6(fnet::Ipv6Address {
                        addr: packet.body().group_addr.ipv6_bytes(),
                    }))
                },
            )
        }
        packet_formats::icmp::mld::MldPacket::MulticastListenerQuery(_) => None,
    }
}

/// Verifies that the `expected_event` occurred on the `fake_endpoint`.
async fn expect_multicast_event(
    fake_endpoint: &netemul::TestFakeEndpoint<'_>,
    expected_event: MulticastEvent,
) {
    let expected_event = &expected_event;
    let stream = fake_endpoint
        .frame_stream()
        .map(|r| r.expect("error getting OnData event"))
        .filter_map(|(data, _dropped)| async move {
            let mut data = &data[..];
            let eth = packet_formats::ethernet::EthernetFrame::parse(
                &mut data,
                // Do not check the frame length as the size of IGMP reports may
                // be less than the minimum ethernet frame length and our
                // virtual (netemul) interface does not pad runt ethernet frames
                // before transmission.
                packet_formats::ethernet::EthernetFrameLengthCheck::NoCheck,
            )
            .expect("failed to parse ethernet frame");

            let event = match eth.ethertype().expect("ethertype missing from ethernet frame") {
                packet_formats::ethernet::EtherType::Ipv4 => extract_v4_multicast_event(data),
                packet_formats::ethernet::EtherType::Ipv6 => extract_v6_multicast_event(data),
                packet_formats::ethernet::EtherType::Arp => None,
                packet_formats::ethernet::EtherType::Other(_other) => None,
            };

            // The same event may be emitted multiple times. As a result, we
            // must wait for the expected event.
            event.and_then(|event| (event == *expected_event).then(|| ()))
        });
    futures::pin_mut!(stream);
    stream.next().await.expect("failed to find expected multicast event");
}

#[test_case(
    "ipv4",
    fnet::IpAddress::Ipv4(DEFAULT_IPV4_MULTICAST_ADDRESS),
    DEFAULT_IPV4_SOURCE_SUBNET;
    "ipv4")]
#[test_case(
    "ipv6",
    fnet::IpAddress::Ipv6(DEFAULT_IPV6_MULTICAST_ADDRESS),
    DEFAULT_IPV6_LINK_LOCAL_SOURCE_SUBNET;
    "ipv6")]
#[fuchsia_async::run_singlethreaded(test)]
async fn join_multicast_group(
    name: &str,
    mut multicast_address: fnet::IpAddress,
    subnet: fnet::Subnet,
) {
    let sandbox = netemul::TestSandbox::new().expect("failed to create sandbox");
    let network = sandbox.create_network("network").await.expect("failed to create network");
    let realm = create_netstack_realm(format!("join_multicast_group_{}", name), &sandbox)
        .expect("failed to create netstack realm");
    let fake_ep = network.create_fake_endpoint().expect("failed to create fake endpoint");

    let network_test_realm = realm
        .connect_to_protocol::<fntr::ControllerMarker>()
        .expect("failed to connect to network test realm controller");

    network_test_realm
        .start_hermetic_network_realm(fntr::Netstack::V2)
        .await
        .expect("start_hermetic_network_realm failed")
        .expect("start_hermetic_network_realm error");

    let _interface = join_network_with_hermetic_netstack(
        &realm,
        &network,
        &network_test_realm,
        ETH1_INTERFACE_NAME,
        ETH1_MAC_ADDRESS,
        subnet,
    )
    .await;

    network_test_realm
        .join_multicast_group(
            &mut multicast_address,
            expect_hermetic_interface_id(ETH1_INTERFACE_NAME, &realm).await,
        )
        .await
        .expect("join_multicast_group failed")
        .expect("join_multicast_group error");

    expect_multicast_event(&fake_ep, MulticastEvent::Joined(multicast_address)).await;
}

// Tests that the persisted multicast socket is cleared when the hermetic
// network realm is stopped.
#[test_case(
    "ipv4",
    fnet::IpAddress::Ipv4(DEFAULT_IPV4_MULTICAST_ADDRESS),
    fnet::IpAddress::Ipv4(fidl_ip_v4!("224.1.2.4")),
    DEFAULT_IPV4_SOURCE_SUBNET;
    "ipv4")]
#[test_case(
    "ipv6",
    fnet::IpAddress::Ipv6(DEFAULT_IPV6_MULTICAST_ADDRESS),
    fnet::IpAddress::Ipv6(fidl_ip_v6!("ff02::4")),
    DEFAULT_IPV6_LINK_LOCAL_SOURCE_SUBNET;
    "ipv6")]
#[fuchsia_async::run_singlethreaded(test)]
async fn join_multicast_group_after_stop(
    name: &str,
    mut multicast_address: fnet::IpAddress,
    mut second_multicast_address: fnet::IpAddress,
    subnet: fnet::Subnet,
) {
    let sandbox = netemul::TestSandbox::new().expect("failed to create sandbox");
    let network = sandbox.create_network("network").await.expect("failed to create network");
    let realm =
        create_netstack_realm(format!("join_multicast_group_after_stop_{}", name), &sandbox)
            .expect("failed to create netstack realm");
    let fake_ep = network.create_fake_endpoint().expect("failed to create fake endpoint");

    let network_test_realm = realm
        .connect_to_protocol::<fntr::ControllerMarker>()
        .expect("failed to connect to network test realm controller");

    network_test_realm
        .start_hermetic_network_realm(fntr::Netstack::V2)
        .await
        .expect("start_hermetic_network_realm failed")
        .expect("start_hermetic_network_realm error");

    let _interface = join_network_with_hermetic_netstack(
        &realm,
        &network,
        &network_test_realm,
        ETH1_INTERFACE_NAME,
        ETH1_MAC_ADDRESS,
        subnet,
    )
    .await;

    network_test_realm
        .join_multicast_group(
            &mut multicast_address,
            expect_hermetic_interface_id(ETH1_INTERFACE_NAME, &realm).await,
        )
        .await
        .expect("join_multicast_group failed")
        .expect("join_multicast_group error");

    network_test_realm
        .stop_hermetic_network_realm()
        .await
        .expect("stop_hermetic_network_realm failed")
        .expect("stop_hermetic_network_realm error");

    network_test_realm
        .start_hermetic_network_realm(fntr::Netstack::V2)
        .await
        .expect("start_hermetic_network_realm failed")
        .expect("start_hermetic_network_realm error");

    let _interface = join_network_with_hermetic_netstack(
        &realm,
        &network,
        &network_test_realm,
        ETH2_INTERFACE_NAME,
        ETH2_MAC_ADDRESS,
        subnet,
    )
    .await;

    network_test_realm
        .join_multicast_group(
            &mut second_multicast_address,
            expect_hermetic_interface_id(ETH2_INTERFACE_NAME, &realm).await,
        )
        .await
        .expect("join_multicast_group failed")
        .expect("join_multicast_group error");

    expect_multicast_event(&fake_ep, MulticastEvent::Joined(second_multicast_address)).await;
}

#[test_case(
    "ipv4",
    fnet::IpAddress::Ipv4(DEFAULT_IPV4_MULTICAST_ADDRESS),
    DEFAULT_IPV4_SOURCE_SUBNET;
    "ipv4")]
#[test_case(
    "ipv6",
    fnet::IpAddress::Ipv6(DEFAULT_IPV6_MULTICAST_ADDRESS),
    DEFAULT_IPV6_LINK_LOCAL_SOURCE_SUBNET;
    "ipv6")]
#[fuchsia_async::run_singlethreaded(test)]
async fn leave_multicast_group(
    name: &str,
    mut multicast_address: fnet::IpAddress,
    subnet: fnet::Subnet,
) {
    let sandbox = netemul::TestSandbox::new().expect("failed to create sandbox");
    let network = sandbox.create_network("network").await.expect("failed to create network");
    let realm = create_netstack_realm(format!("leave_multicast_group_{}", name), &sandbox)
        .expect("failed to create netstack realm");
    let fake_ep = network.create_fake_endpoint().expect("failed to create fake endpoint");

    let network_test_realm = realm
        .connect_to_protocol::<fntr::ControllerMarker>()
        .expect("failed to connect to network test realm controller");

    network_test_realm
        .start_hermetic_network_realm(fntr::Netstack::V2)
        .await
        .expect("start_hermetic_network_realm failed")
        .expect("start_hermetic_network_realm error");

    let _interface = join_network_with_hermetic_netstack(
        &realm,
        &network,
        &network_test_realm,
        ETH1_INTERFACE_NAME,
        ETH1_MAC_ADDRESS,
        subnet,
    )
    .await;

    let id = expect_hermetic_interface_id(ETH1_INTERFACE_NAME, &realm).await;

    network_test_realm
        .join_multicast_group(&mut multicast_address, id)
        .await
        .expect("join_multicast_group failed")
        .expect("join_multicast_group error");

    network_test_realm
        .leave_multicast_group(&mut multicast_address, id)
        .await
        .expect("leave_multicast_group failed")
        .expect("leave_multicast_group error");

    expect_multicast_event(&fake_ep, MulticastEvent::Left(multicast_address)).await;
}

#[fuchsia_async::run_singlethreaded(test)]
async fn join_multicast_group_with_no_hermetic_network_realm() {
    let sandbox = netemul::TestSandbox::new().expect("failed to create sandbox");
    let realm =
        create_netstack_realm("join_multicast_group_with_no_hermetic_network_realm", &sandbox)
            .expect("failed to create netstack realm");

    let network_test_realm = realm
        .connect_to_protocol::<fntr::ControllerMarker>()
        .expect("failed to connect to network test realm controller");

    assert_eq!(
        network_test_realm
            .join_multicast_group(
                &mut fnet::IpAddress::Ipv4(DEFAULT_IPV4_MULTICAST_ADDRESS),
                DEFAULT_INTERFACE_ID
            )
            .await
            .expect("join_multicast_group failed"),
        Err(fntr::Error::HermeticNetworkRealmNotRunning),
    );
}

#[fuchsia_async::run_singlethreaded(test)]
async fn join_multicast_group_with_non_existent_interface() {
    let sandbox = netemul::TestSandbox::new().expect("failed to create sandbox");
    let realm = create_netstack_realm("join_multicast_group_with_non_existent_interface", &sandbox)
        .expect("failed to create netstack realm");

    let network_test_realm = realm
        .connect_to_protocol::<fntr::ControllerMarker>()
        .expect("failed to connect to network test realm controller");

    network_test_realm
        .start_hermetic_network_realm(fntr::Netstack::V2)
        .await
        .expect("start_hermetic_network_realm failed")
        .expect("start_hermetic_network_realm error");

    assert_eq!(
        network_test_realm
            .join_multicast_group(
                &mut fnet::IpAddress::Ipv4(DEFAULT_IPV4_MULTICAST_ADDRESS),
                // This interface id does not exist. As a result, an error
                // should be returned.
                DEFAULT_INTERFACE_ID
            )
            .await
            .expect("join_multicast_group failed"),
        Err(fntr::Error::InvalidArguments),
    );
}

#[test_case(
    "ipv4",
    DEFAULT_IPV4_SOURCE_SUBNET;
    "ipv4")]
#[test_case(
    "ipv6",
    DEFAULT_IPV6_LINK_LOCAL_SOURCE_SUBNET;
    "ipv6")]
#[fuchsia_async::run_singlethreaded(test)]
async fn join_multicast_group_with_non_multicast_address(name: &str, subnet: fnet::Subnet) {
    let sandbox = netemul::TestSandbox::new().expect("failed to create sandbox");
    let network = sandbox.create_network("network").await.expect("failed to create network");
    let realm = create_netstack_realm(
        format!("join_multicast_group_with_non_multicast_address_{}", name),
        &sandbox,
    )
    .expect("failed to create netstack realm");

    let network_test_realm = realm
        .connect_to_protocol::<fntr::ControllerMarker>()
        .expect("failed to connect to network test realm controller");

    network_test_realm
        .start_hermetic_network_realm(fntr::Netstack::V2)
        .await
        .expect("start_hermetic_network_realm failed")
        .expect("start_hermetic_network_realm error");

    let _interface = join_network_with_hermetic_netstack(
        &realm,
        &network,
        &network_test_realm,
        ETH1_INTERFACE_NAME,
        ETH1_MAC_ADDRESS,
        subnet,
    )
    .await;

    // `address` is not within the multicast address range. Therefore, an error
    // should be returned.
    let mut address = subnet.addr;
    assert_eq!(
        network_test_realm
            .join_multicast_group(
                &mut address,
                expect_hermetic_interface_id(ETH1_INTERFACE_NAME, &realm).await
            )
            .await
            .expect("join_multicast_group failed"),
        Err(fntr::Error::InvalidArguments),
    );
}

#[test_case(
    "ipv4",
    fnet::IpAddress::Ipv4(DEFAULT_IPV4_MULTICAST_ADDRESS),
    DEFAULT_IPV4_SOURCE_SUBNET;
    "ipv4")]
#[test_case(
    "ipv6",
    fnet::IpAddress::Ipv6(DEFAULT_IPV6_MULTICAST_ADDRESS),
    DEFAULT_IPV6_LINK_LOCAL_SOURCE_SUBNET;
    "ipv6")]
#[fuchsia_async::run_singlethreaded(test)]
async fn join_same_multicast_group_multiple_times(
    name: &str,
    mut multicast_address: fnet::IpAddress,
    subnet: fnet::Subnet,
) {
    let sandbox = netemul::TestSandbox::new().expect("failed to create sandbox");
    let network = sandbox.create_network("network").await.expect("failed to create network");
    let realm = create_netstack_realm(
        format!("join_same_multicast_group_multiple_times_{}", name),
        &sandbox,
    )
    .expect("failed to create netstack realm");

    let network_test_realm = realm
        .connect_to_protocol::<fntr::ControllerMarker>()
        .expect("failed to connect to network test realm controller");

    network_test_realm
        .start_hermetic_network_realm(fntr::Netstack::V2)
        .await
        .expect("start_hermetic_network_realm failed")
        .expect("start_hermetic_network_realm error");

    let _interface = join_network_with_hermetic_netstack(
        &realm,
        &network,
        &network_test_realm,
        ETH1_INTERFACE_NAME,
        ETH1_MAC_ADDRESS,
        subnet,
    )
    .await;

    let id = expect_hermetic_interface_id(ETH1_INTERFACE_NAME, &realm).await;
    network_test_realm
        .join_multicast_group(&mut multicast_address, id)
        .await
        .expect("join_multicast_group failed")
        .expect("join_multicast_group error");

    // Verify that the error is propagated whenever the same multicast group is
    // joined multiple times.
    assert_eq!(
        network_test_realm
            .join_multicast_group(&mut multicast_address, id)
            .await
            .expect("duplicate join_multicast_group failed"),
        Err(fntr::Error::AddressInUse)
    );
}

#[fuchsia_async::run_singlethreaded(test)]
async fn leave_multicast_group_with_no_hermetic_network_realm() {
    let sandbox = netemul::TestSandbox::new().expect("failed to create sandbox");
    let realm =
        create_netstack_realm("leave_multicast_group_with_no_hermetic_network_realm", &sandbox)
            .expect("failed to create netstack realm");

    let network_test_realm = realm
        .connect_to_protocol::<fntr::ControllerMarker>()
        .expect("failed to connect to network test realm controller");

    assert_eq!(
        network_test_realm
            .leave_multicast_group(
                &mut fnet::IpAddress::Ipv4(DEFAULT_IPV4_MULTICAST_ADDRESS),
                DEFAULT_INTERFACE_ID
            )
            .await
            .expect("leave_multicast_group failed"),
        Err(fntr::Error::HermeticNetworkRealmNotRunning),
    );
}

#[test_case(
    "ipv4",
    DEFAULT_IPV4_SOURCE_SUBNET;
    "ipv4")]
#[test_case(
    "ipv6",
    DEFAULT_IPV6_LINK_LOCAL_SOURCE_SUBNET;
    "ipv6")]
#[fuchsia_async::run_singlethreaded(test)]
async fn leave_multicast_group_with_non_multicast_address(name: &str, subnet: fnet::Subnet) {
    let sandbox = netemul::TestSandbox::new().expect("failed to create sandbox");
    let network = sandbox.create_network("network").await.expect("failed to create network");
    let realm = create_netstack_realm(
        format!("leave_multicast_group_with_non_multicast_address_{}", name),
        &sandbox,
    )
    .expect("failed to create netstack realm");

    let network_test_realm = realm
        .connect_to_protocol::<fntr::ControllerMarker>()
        .expect("failed to connect to network test realm controller");

    network_test_realm
        .start_hermetic_network_realm(fntr::Netstack::V2)
        .await
        .expect("start_hermetic_network_realm failed")
        .expect("start_hermetic_network_realm error");

    let _interface = join_network_with_hermetic_netstack(
        &realm,
        &network,
        &network_test_realm,
        ETH1_INTERFACE_NAME,
        ETH1_MAC_ADDRESS,
        subnet,
    )
    .await;

    // `address` is not within the multicast address range. Therefore, an error
    // should be returned.
    let mut address = subnet.addr;
    assert_eq!(
        network_test_realm
            .leave_multicast_group(
                &mut address,
                expect_hermetic_interface_id(ETH1_INTERFACE_NAME, &realm).await
            )
            .await
            .expect("leave_multicast_group failed"),
        Err(fntr::Error::InvalidArguments),
    );
}

#[test_case(
    "ipv4",
    fnet::IpAddress::Ipv4(DEFAULT_IPV4_MULTICAST_ADDRESS),
    DEFAULT_IPV4_SOURCE_SUBNET;
    "ipv4")]
#[test_case(
    "ipv6",
    fnet::IpAddress::Ipv6(DEFAULT_IPV6_MULTICAST_ADDRESS),
    DEFAULT_IPV6_LINK_LOCAL_SOURCE_SUBNET;
    "ipv6")]
#[fuchsia_async::run_singlethreaded(test)]
async fn leave_unjoined_multicast_group(
    name: &str,
    mut multicast_address: fnet::IpAddress,
    subnet: fnet::Subnet,
) {
    let sandbox = netemul::TestSandbox::new().expect("failed to create sandbox");
    let network = sandbox.create_network("network").await.expect("failed to create network");
    let realm = create_netstack_realm(format!("leave_unjoined_multicast_group_{}", name), &sandbox)
        .expect("failed to create netstack realm");

    let network_test_realm = realm
        .connect_to_protocol::<fntr::ControllerMarker>()
        .expect("failed to connect to network test realm controller");

    network_test_realm
        .start_hermetic_network_realm(fntr::Netstack::V2)
        .await
        .expect("start_hermetic_network_realm failed")
        .expect("start_hermetic_network_realm error");

    let _interface = join_network_with_hermetic_netstack(
        &realm,
        &network,
        &network_test_realm,
        ETH1_INTERFACE_NAME,
        ETH1_MAC_ADDRESS,
        subnet,
    )
    .await;

    // The multicast group must be joined before it can be left.
    assert_eq!(
        network_test_realm
            .leave_multicast_group(
                &mut multicast_address,
                expect_hermetic_interface_id(ETH1_INTERFACE_NAME, &realm).await
            )
            .await
            .expect("leave_multicast_group failed"),
        Err(fntr::Error::AddressNotAvailable)
    );
}
