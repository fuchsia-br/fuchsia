// Copyright 2021 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/devices/misc/drivers/compat/device.h"

#include <fidl/fuchsia.device/cpp/markers.h>
#include <fidl/fuchsia.driver.framework/cpp/wire_test_base.h>
#include <lib/ddk/metadata.h>
#include <lib/gtest/test_loop_fixture.h>

#include <gtest/gtest.h>

#include "lib/ddk/binding_priv.h"
#include "lib/ddk/device.h"
#include "src/devices/misc/drivers/compat/devfs_vnode.h"

namespace fdf = fuchsia_driver_framework;
namespace fio = fuchsia_io;
namespace frunner = fuchsia_component_runner;

namespace {

class TestNode : public fidl::testing::WireTestBase<fdf::Node> {
 public:
  void Clear() {
    controllers_.clear();
    nodes_.clear();
  }

  void SetAddChildHook(std::function<void(AddChildRequestView& rv)> func) {
    add_child_hook_.emplace(std::move(func));
  }

 private:
  void AddChild(AddChildRequestView request, AddChildCompleter::Sync& completer) override {
    if (add_child_hook_) {
      add_child_hook_.value()(request);
    }
    controllers_.push_back(std::move(request->controller));
    nodes_.push_back(std::move(request->node));
    completer.ReplySuccess();
  }

  void NotImplemented_(const std::string& name, fidl::CompleterBase& completer) override {
    printf("Not implemented: Node::%s\n", name.data());
  }

  std::optional<std::function<void(AddChildRequestView& rv)>> add_child_hook_;
  std::vector<fidl::ServerEnd<fdf::NodeController>> controllers_;
  std::vector<fidl::ServerEnd<fdf::Node>> nodes_;
};

}  // namespace

class DeviceTest : public gtest::TestLoopFixture {
 public:
  void SetUp() override {
    TestLoopFixture::SetUp();

    auto svc = fidl::CreateEndpoints<fio::Directory>();
    ASSERT_EQ(ZX_OK, svc.status_value());
    auto ns = CreateNamespace(std::move(svc->client));
    ASSERT_EQ(ZX_OK, ns.status_value());

    auto logger = driver::Logger::Create(*ns, dispatcher(), "test-logger");
    ASSERT_EQ(ZX_OK, logger.status_value());
    logger_ = std::move(*logger);
  }

 protected:
  driver::Logger& logger() { return logger_; }

 private:
  zx::status<driver::Namespace> CreateNamespace(fidl::ClientEnd<fio::Directory> client_end) {
    fidl::Arena arena;
    fidl::VectorView<frunner::wire::ComponentNamespaceEntry> entries(arena, 1);
    entries[0].Allocate(arena);
    entries[0].set_path(arena, "/svc").set_directory(std::move(client_end));
    return driver::Namespace::Create(entries);
  }

  driver::Logger logger_;
};

TEST_F(DeviceTest, ConstructDevice) {
  auto endpoints = fidl::CreateEndpoints<fdf::Node>();

  // Create a device.
  zx_protocol_device_t ops{};
  compat::Device device("test-device", nullptr, {}, &ops, nullptr, std::nullopt, logger(),
                        dispatcher());
  device.Bind({std::move(endpoints->client), dispatcher()});

  // Test basic functions on the device.
  EXPECT_EQ(reinterpret_cast<uintptr_t>(&device), reinterpret_cast<uintptr_t>(device.ZxDevice()));
  EXPECT_STREQ("test-device", device.Name());
  EXPECT_FALSE(device.HasChildren());

  // Create a node to test device unbind.
  TestNode node;
  fidl::BindServer(dispatcher(), std::move(endpoints->server), &node,
                   [](auto, fidl::UnbindInfo info, auto) {
                     EXPECT_EQ(fidl::Reason::kPeerClosed, info.reason());
                   });
  device.Unbind();

  ASSERT_TRUE(RunLoopUntilIdle());
}

TEST_F(DeviceTest, AddChildDevice) {
  auto endpoints = fidl::CreateEndpoints<fdf::Node>();

  // Create a node.
  TestNode node;
  auto binding = fidl::BindServer(dispatcher(), std::move(endpoints->server), &node);

  // Create a device.
  zx_protocol_device_t ops{};
  compat::Device parent("parent", nullptr, {}, &ops, nullptr, std::nullopt, logger(), dispatcher());
  parent.Bind({std::move(endpoints->client), dispatcher()});

  // Add a child device.
  device_add_args_t args{.name = "child"};
  zx_device_t* child = nullptr;
  zx_status_t status = parent.Add(&args, &child);
  ASSERT_EQ(ZX_OK, status);
  EXPECT_NE(nullptr, child);
  EXPECT_STREQ("child", child->Name());
  EXPECT_TRUE(parent.HasChildren());

  // Ensure that AddChild was executed.
  ASSERT_TRUE(RunLoopUntilIdle());
}

TEST_F(DeviceTest, AddChildWithProtoPropAndProtoId) {
  auto endpoints = fidl::CreateEndpoints<fdf::Node>();

  // Create a node.
  TestNode node;
  auto binding = fidl::BindServer(dispatcher(), std::move(endpoints->server), &node);

  // Create a device.
  zx_protocol_device_t ops{};
  compat::Device parent("parent", nullptr, {}, &ops, nullptr, std::nullopt, logger(), dispatcher());
  parent.Bind({std::move(endpoints->client), dispatcher()});

  bool ran = false;
  node.SetAddChildHook([&ran](TestNode::AddChildRequestView& rv) {
    ran = true;
    auto& prop = rv->args.properties()[0];
    ASSERT_EQ(prop.key().int_value(), (uint32_t)BIND_PROTOCOL);
    ASSERT_EQ(prop.value().int_value(), ZX_PROTOCOL_I2C);
  });

  // Add a child device.
  zx_device_prop_t prop{.id = BIND_PROTOCOL, .value = ZX_PROTOCOL_I2C};
  device_add_args_t args{
      .name = "child", .props = &prop, .prop_count = 1, .proto_id = ZX_PROTOCOL_BLOCK};
  zx_device_t* child = nullptr;
  zx_status_t status = parent.Add(&args, &child);
  ASSERT_EQ(ZX_OK, status);
  EXPECT_NE(nullptr, child);
  EXPECT_STREQ("child", child->Name());
  EXPECT_TRUE(parent.HasChildren());

  ASSERT_TRUE(RunLoopUntilIdle());
  ASSERT_TRUE(ran);
}

TEST_F(DeviceTest, AddChildDeviceWithInit) {
  auto endpoints = fidl::CreateEndpoints<fdf::Node>();

  // Create a node.
  TestNode node;
  auto binding = fidl::BindServer(dispatcher(), std::move(endpoints->server), &node);

  // Create a device.
  zx_protocol_device_t parent_ops{};
  compat::Device parent("parent", nullptr, {}, &parent_ops, nullptr, std::nullopt, logger(),
                        dispatcher());
  parent.Bind({std::move(endpoints->client), dispatcher()});

  // Add a child device.
  bool child_ctx = false;
  static zx_protocol_device_t child_ops{
      .init = [](void* ctx) { *static_cast<bool*>(ctx) = true; },
  };
  device_add_args_t args{
      .name = "child",
      .ctx = &child_ctx,
      .ops = &child_ops,
  };
  zx_device_t* child = nullptr;
  zx_status_t status = parent.Add(&args, &child);
  ASSERT_EQ(ZX_OK, status);
  EXPECT_NE(nullptr, child);
  EXPECT_STREQ("child", child->Name());
  EXPECT_TRUE(parent.HasChildren());

  // Check that the init hook was run.
  EXPECT_FALSE(child_ctx);
  ASSERT_TRUE(RunLoopUntilIdle());
  EXPECT_TRUE(child_ctx);
}

TEST_F(DeviceTest, AddAndRemoveChildDevice) {
  auto endpoints = fidl::CreateEndpoints<fdf::Node>();

  // Create a node.
  TestNode node;
  auto binding = fidl::BindServer(dispatcher(), std::move(endpoints->server), &node);

  // Create a device.
  zx_protocol_device_t ops{};
  compat::Device parent("parent", nullptr, {}, &ops, nullptr, std::nullopt, logger(), dispatcher());
  parent.Bind({std::move(endpoints->client), dispatcher()});

  // Add a child device.
  device_add_args_t args{.name = "child"};
  zx_device_t* child = nullptr;
  zx_status_t status = parent.Add(&args, &child);
  ASSERT_EQ(ZX_OK, status);
  EXPECT_NE(nullptr, child);
  EXPECT_STREQ("child", child->Name());
  EXPECT_TRUE(parent.HasChildren());

  // Remove the child device.
  child->Remove();
  ASSERT_TRUE(RunLoopUntilIdle());

  // Emulate the removal of the node, and check that the related child device is
  // removed from the parent device.
  EXPECT_TRUE(parent.HasChildren());
  node.Clear();
  ASSERT_TRUE(RunLoopUntilIdle());
  EXPECT_FALSE(parent.HasChildren());
}

TEST_F(DeviceTest, GetProtocolFromDevice) {
  // Create a device without a get_protocol hook.
  zx_protocol_device_t ops{};
  compat::Device without("without-protocol", nullptr, {}, &ops, nullptr, std::nullopt, logger(),
                         dispatcher());
  ASSERT_EQ(ZX_ERR_NOT_SUPPORTED, without.GetProtocol(ZX_PROTOCOL_BLOCK, nullptr));

  // Create a device with a get_protocol hook.
  ops.get_protocol = [](void* ctx, uint32_t proto_id, void* protocol) {
    EXPECT_EQ(ZX_PROTOCOL_BLOCK, proto_id);
    return ZX_OK;
  };
  compat::Device with("with-protocol", nullptr, {}, &ops, nullptr, std::nullopt, logger(),
                      dispatcher());
  ASSERT_EQ(ZX_OK, with.GetProtocol(ZX_PROTOCOL_BLOCK, nullptr));
}

TEST_F(DeviceTest, DeviceMetadata) {
  // Create a device.
  zx_protocol_device_t ops{};
  compat::Device device("test-device", nullptr, {}, &ops, nullptr, std::nullopt, logger(),
                        dispatcher());

  // Add metadata to the device.
  const uint64_t metadata = 0xAABBCCDDEEFF0011;
  zx_status_t status = device.AddMetadata(DEVICE_METADATA_PRIVATE, &metadata, sizeof(metadata));
  ASSERT_EQ(ZX_OK, status);

  // Add the same metadata again.
  status = device.AddMetadata(DEVICE_METADATA_PRIVATE, &metadata, sizeof(metadata));
  ASSERT_EQ(ZX_ERR_ALREADY_EXISTS, status);

  // Check the metadata size.
  size_t size = 0;
  status = device.GetMetadataSize(DEVICE_METADATA_PRIVATE, &size);
  ASSERT_EQ(ZX_OK, status);
  EXPECT_EQ(sizeof(metadata), size);

  // Check the metadata size for missing metadata.
  status = device.GetMetadataSize(DEVICE_METADATA_BOARD_PRIVATE, &size);
  ASSERT_EQ(ZX_ERR_NOT_FOUND, status);

  // Get the metadata.
  uint64_t found = 0;
  size_t found_size = 0;
  status = device.GetMetadata(DEVICE_METADATA_PRIVATE, &found, sizeof(found), &found_size);
  ASSERT_EQ(ZX_OK, status);
  EXPECT_EQ(metadata, found);
  EXPECT_EQ(sizeof(metadata), found_size);

  // Get the metadata for missing metadata.
  status = device.GetMetadata(DEVICE_METADATA_BOARD_PRIVATE, &found, sizeof(found), &found_size);
  ASSERT_EQ(ZX_ERR_NOT_FOUND, status);
}

TEST_F(DeviceTest, DeviceFragmentMetadata) {
  // Create a device.
  zx_protocol_device_t ops{};
  compat::Device device("test-device", nullptr, {}, &ops, nullptr, std::nullopt, logger(),
                        dispatcher());

  // Add metadata to the device.
  const uint64_t metadata = 0xAABBCCDDEEFF0011;
  zx_status_t status = device.AddMetadata(DEVICE_METADATA_PRIVATE, &metadata, sizeof(metadata));
  ASSERT_EQ(ZX_OK, status);

  // Get the metadata.
  uint64_t found = 0;
  size_t found_size = 0;
  status = device_get_fragment_metadata(device.ZxDevice(), "fragment-name", DEVICE_METADATA_PRIVATE,
                                        &found, sizeof(found), &found_size);
  ASSERT_EQ(ZX_OK, status);
  EXPECT_EQ(metadata, found);
  EXPECT_EQ(sizeof(metadata), found_size);
}

TEST_F(DeviceTest, GetFragmentProtocolFromDevice) {
  // Create a device with a get_protocol hook.
  zx_protocol_device_t ops{};
  ops.get_protocol = [](void* ctx, uint32_t proto_id, void* protocol) {
    EXPECT_EQ(ZX_PROTOCOL_BLOCK, proto_id);
    return ZX_OK;
  };
  compat::Device with("with-protocol", nullptr, {}, &ops, nullptr, std::nullopt, logger(),
                      dispatcher());
  ASSERT_EQ(ZX_OK, device_get_fragment_protocol(with.ZxDevice(), "fragment-name", ZX_PROTOCOL_BLOCK,
                                                nullptr));
}

TEST_F(DeviceTest, DevfsVnodeGetTopologicalPath) {
  auto endpoints = fidl::CreateEndpoints<fdf::Node>();

  // Create a device.
  zx_protocol_device_t ops{};
  compat::Device device("test-device", nullptr, {}, &ops, nullptr, std::nullopt, logger(),
                        dispatcher());
  device.Bind({std::move(endpoints->client), dispatcher()});

  // The root device doesn't have a valid topological path, so we add a child.
  zx_device_t* second_device;
  device_add_args_t args{
      .name = "second-device",
  };
  device.Add(&args, &second_device);

  DevfsVnode vnode(second_device, device.logger());
  auto dev_endpoints = fidl::CreateEndpoints<fuchsia_device::Controller>();
  ASSERT_EQ(ZX_OK, endpoints.status_value());

  fidl::BindServer(test_loop().dispatcher(), std::move(dev_endpoints->server), &vnode);

  fidl::WireClient<fuchsia_device::Controller> client;
  client.Bind(std::move(dev_endpoints->client), test_loop().dispatcher());

  bool callback_called = false;
  client->GetTopologicalPath(
      [&callback_called](
          fidl::WireResponse<fuchsia_device::Controller::GetTopologicalPath>* response) {
        ASSERT_TRUE(response->result.is_response());
        std::string path(response->result.response().path.data(),
                         response->result.response().path.size());
        EXPECT_STREQ("/dev/second-device", path.data());
        callback_called = true;
      });

  ASSERT_TRUE(test_loop().RunUntilIdle());
  ASSERT_TRUE(callback_called);
}
