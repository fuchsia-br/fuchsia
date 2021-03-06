// Copyright 2021 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/devices/misc/drivers/compat/driver.h"

#include <dirent.h>
#include <fidl/fuchsia.boot/cpp/wire_test_base.h>
#include <fidl/fuchsia.driver.framework/cpp/wire_test_base.h>
#include <fidl/fuchsia.io/cpp/wire_test_base.h>
#include <fidl/fuchsia.logger/cpp/wire.h>
#include <lib/async-loop/cpp/loop.h>
#include <lib/fdio/directory.h>
#include <lib/gtest/test_loop_fixture.h>

#include <fbl/unique_fd.h>
#include <gtest/gtest.h>

#include "src/devices/lib/compat/symbols.h"
#include "src/devices/misc/drivers/compat/v1_test.h"

namespace fboot = fuchsia_boot;
namespace fdata = fuchsia_data;
namespace fdf = fuchsia_driver_framework;
namespace fio = fuchsia_io;
namespace flogger = fuchsia_logger;
namespace fmem = fuchsia_mem;
namespace frunner = fuchsia_component_runner;

constexpr auto kOpenFlags = fio::wire::kOpenRightReadable | fio::wire::kOpenRightExecutable |
                            fio::wire::kOpenFlagNotDirectory;
constexpr auto kVmoFlags = fio::wire::kVmoFlagRead | fio::wire::kVmoFlagExec;

namespace {

fmem::wire::Buffer GetBuffer(std::string_view path) {
  auto endpoints = fidl::CreateEndpoints<fio::File>();
  EXPECT_TRUE(endpoints.is_ok());
  zx_status_t status = fdio_open(path.data(), kOpenFlags, endpoints->server.channel().release());
  EXPECT_EQ(ZX_OK, status);
  auto result = fidl::WireCall(endpoints->client)->GetBuffer(kVmoFlags);
  EXPECT_EQ(ZX_OK, result->s);
  return std::move(*result->buffer);
}

class TestNode : public fidl::testing::WireTestBase<fdf::Node> {
 public:
  bool HasChildren() const { return !controllers_.empty() || !nodes_.empty(); }

 private:
  void AddChild(AddChildRequestView request, AddChildCompleter::Sync& completer) override {
    controllers_.push_back(std::move(request->controller));
    nodes_.push_back(std::move(request->node));
    completer.ReplySuccess();
  }

  void NotImplemented_(const std::string& name, fidl::CompleterBase& completer) override {
    printf("Not implemented: Node::%s\n", name.data());
  }

  std::vector<fidl::ServerEnd<fdf::NodeController>> controllers_;
  std::vector<fidl::ServerEnd<fdf::Node>> nodes_;
};

class TestRootResource : public fidl::testing::WireTestBase<fboot::RootResource> {
 public:
  TestRootResource() { EXPECT_EQ(ZX_OK, zx::event::create(0, &fake_resource_)); }

 private:
  void Get(GetRequestView request, GetCompleter::Sync& completer) override {
    zx::event duplicate;
    ASSERT_EQ(ZX_OK, fake_resource_.duplicate(ZX_RIGHT_SAME_RIGHTS, &duplicate));
    completer.Reply(zx::resource(duplicate.release()));
  }

  void NotImplemented_(const std::string& name, fidl::CompleterBase& completer) override {
    printf("Not implemented: RootResource::%s\n", name.data());
    completer.Close(ZX_ERR_NOT_SUPPORTED);
  }

  // An event is similar enough that we can pretend it's the root resource, in that we can
  // send it over a FIDL channel.
  zx::event fake_resource_;
};

class TestItems : public fidl::testing::WireTestBase<fboot::Items> {
 private:
  void NotImplemented_(const std::string& name, fidl::CompleterBase& completer) override {
    printf("Not implemented: Items::%s\n", name.data());
    completer.Close(ZX_ERR_NOT_SUPPORTED);
  }
};

class TestFile : public fidl::testing::WireTestBase<fio::File> {
 public:
  void SetStatus(zx_status_t status) { status_ = status; }
  void SetBuffer(fmem::wire::Buffer buffer) { buffer_ = std::move(buffer); }

 private:
  void GetBuffer(GetBufferRequestView request, GetBufferCompleter::Sync& completer) override {
    completer.Reply(status_, fidl::ObjectView<fmem::wire::Buffer>::FromExternal(&buffer_));
  }

  void NotImplemented_(const std::string& name, fidl::CompleterBase& completer) override {
    printf("Not implemented: File::%s\n", name.data());
    completer.Close(ZX_ERR_NOT_SUPPORTED);
  }

  zx_status_t status_ = ZX_OK;
  fmem::wire::Buffer buffer_;
};

class TestDirectory : public fidl::testing::WireTestBase<fio::Directory> {
 public:
  using OpenHandler = fit::function<void(OpenRequestView)>;

  void SetOpenHandler(OpenHandler open_handler) { open_handler_ = std::move(open_handler); }

 private:
  void Open(OpenRequestView request, OpenCompleter::Sync& completer) override {
    open_handler_(std::move(request));
  }

  void NotImplemented_(const std::string& name, fidl::CompleterBase& completer) override {
    printf("Not implemented: Directory::%s\n", name.data());
    completer.Close(ZX_ERR_NOT_SUPPORTED);
  }

  OpenHandler open_handler_;
};

class TestDevice : public fidl::WireServer<fuchsia_driver_compat::Device> {
  void GetTopologicalPath(GetTopologicalPathRequestView request,
                          GetTopologicalPathCompleter::Sync& completer) override {
    completer.Reply("/dev/test/my-device");
  }

  void GetMetadata(GetMetadataRequestView request, GetMetadataCompleter::Sync& completer) override {
    std::vector<fuchsia_driver_compat::wire::Metadata> metadata;

    std::vector<uint8_t> bytes_1 = {1, 2, 3};
    zx::vmo vmo_1;
    ASSERT_EQ(ZX_OK, zx::vmo::create(bytes_1.size(), 0, &vmo_1));
    vmo_1.write(bytes_1.data(), 0, bytes_1.size());
    size_t size = bytes_1.size();
    ASSERT_EQ(ZX_OK, vmo_1.set_property(ZX_PROP_VMO_CONTENT_SIZE, &size, sizeof(size)));
    metadata.push_back(fuchsia_driver_compat::wire::Metadata{.type = 1, .data = std::move(vmo_1)});

    std::vector<uint8_t> bytes_2 = {4, 5, 6};
    zx::vmo vmo_2;
    ASSERT_EQ(ZX_OK, zx::vmo::create(bytes_1.size(), 0, &vmo_2));
    vmo_2.write(bytes_2.data(), 0, bytes_2.size());
    ASSERT_EQ(ZX_OK, vmo_2.set_property(ZX_PROP_VMO_CONTENT_SIZE, &size, sizeof(size)));
    metadata.push_back(fuchsia_driver_compat::wire::Metadata{.type = 2, .data = std::move(vmo_2)});

    completer.ReplySuccess(fidl::VectorView<fuchsia_driver_compat::wire::Metadata>::FromExternal(
        metadata.data(), metadata.size()));
  }
};

}  // namespace

class DriverTest : public gtest::TestLoopFixture {
 protected:
  TestNode& node() { return node_; }
  TestFile& compat_file() { return compat_file_; }

  std::unique_ptr<compat::Driver> StartDriver(std::string_view v1_driver_path,
                                              const zx_protocol_device_t* ops) {
    auto node_endpoints = fidl::CreateEndpoints<fdf::Node>();
    EXPECT_TRUE(node_endpoints.is_ok());
    auto outgoing_dir_endpoints = fidl::CreateEndpoints<fio::Directory>();
    EXPECT_TRUE(outgoing_dir_endpoints.is_ok());
    auto pkg_endpoints = fidl::CreateEndpoints<fio::Directory>();
    EXPECT_TRUE(pkg_endpoints.is_ok());
    auto svc_endpoints = fidl::CreateEndpoints<fio::Directory>();
    EXPECT_TRUE(svc_endpoints.is_ok());
    auto compat_service_endpoints = fidl::CreateEndpoints<fio::Directory>();
    EXPECT_TRUE(compat_service_endpoints.is_ok());

    // Setup the node.
    fidl::WireSharedClient<fuchsia_driver_framework::Node> node(std::move(node_endpoints->client),
                                                                dispatcher());
    fidl::BindServer(dispatcher(), std::move(node_endpoints->server), &node_);

    // Setup and bind "/pkg" directory.
    compat_file_.SetBuffer(GetBuffer("/pkg/driver/compat.so"));
    v1_test_file_.SetBuffer(GetBuffer(v1_driver_path));
    firmware_file_.SetBuffer(GetBuffer("/pkg/lib/firmware/test"));
    pkg_directory_.SetOpenHandler([this](TestDirectory::OpenRequestView request) {
      fidl::ServerEnd<fio::File> server_end(request->object.TakeChannel());
      if (request->path.get() == "driver/compat.so") {
        fidl::BindServer(dispatcher(), std::move(server_end), &compat_file_);
      } else if (request->path.get() == "driver/v1_test.so") {
        fidl::BindServer(dispatcher(), std::move(server_end), &v1_test_file_);
      } else if (request->path.get() == "lib/firmware/test") {
        fidl::BindServer(dispatcher(), std::move(server_end), &firmware_file_);
      } else {
        FAIL() << "Unexpected file: " << request->path.get();
      }
    });
    fidl::BindServer(dispatcher(), std::move(pkg_endpoints->server), &pkg_directory_);

    // Setup and bind "/fuchsia.driver.compat.Service" directory.
    compat_service_directory_.SetOpenHandler([this](TestDirectory::OpenRequestView request) {
      // We cheat here. Instead of creating a second TestDirectory for 'default', we open ourselves
      // again.
      if (request->path.get() == "default") {
        fidl::ServerEnd<fio::Directory> server_end(request->object.TakeChannel());
        fidl::BindServer(dispatcher(), std::move(server_end), &compat_service_directory_);
      } else if (request->path.get() == "device") {
        fidl::ServerEnd<fuchsia_driver_compat::Device> server_end(request->object.TakeChannel());
        fidl::BindServer(dispatcher(), std::move(server_end), &test_device_);
      } else {
        FAIL() << "Unexpected service: " << request->path.get();
      }
    });
    fidl::BindServer(dispatcher(), std::move(compat_service_endpoints->server),
                     &compat_service_directory_);

    // Setup and bind "/svc" directory.
    svc_directory_.SetOpenHandler([this](TestDirectory::OpenRequestView request) {
      if (request->path.get() == fidl::DiscoverableProtocolName<flogger::LogSink>) {
        zx_status_t status = fdio_service_connect_by_name(
            fidl::DiscoverableProtocolName<flogger::LogSink>, request->object.channel().release());
        ASSERT_EQ(ZX_OK, status);
      } else if (request->path.get() == fidl::DiscoverableProtocolName<fboot::RootResource>) {
        fidl::ServerEnd<fboot::RootResource> server_end(request->object.TakeChannel());
        fidl::BindServer(dispatcher(), std::move(server_end), &root_resource_);
      } else if (request->path.get() == fidl::DiscoverableProtocolName<fboot::Items>) {
        fidl::ServerEnd<fboot::Items> server_end(request->object.TakeChannel());
        fidl::BindServer(dispatcher(), std::move(server_end), &items_);
      } else if (request->path.get() ==
                 fidl::DiscoverableProtocolName<fuchsia_device_fs::Exporter>) {
        // TODO: If the unit tests need a working Exporter add it here.
      } else {
        FAIL() << "Unexpected service: " << request->path.get();
      }
    });
    fidl::BindServer(dispatcher(), std::move(svc_endpoints->server), &svc_directory_);

    // Setup the namespace.
    fidl::Arena arena;
    fidl::VectorView<frunner::wire::ComponentNamespaceEntry> ns_entries(arena, 3);
    ns_entries[0].Allocate(arena);
    ns_entries[0].set_path(arena, "/pkg");
    ns_entries[0].set_directory(std::move(pkg_endpoints->client));
    ns_entries[1].Allocate(arena);
    ns_entries[1].set_path(arena, "/svc");
    ns_entries[1].set_directory(std::move(svc_endpoints->client));
    ns_entries[2].Allocate(arena);
    ns_entries[2].set_path(arena, "/fuchsia.driver.compat.Service");
    ns_entries[2].set_directory(std::move(compat_service_endpoints->client));
    auto ns = driver::Namespace::Create(ns_entries);
    EXPECT_EQ(ZX_OK, ns.status_value());

    // Setup the logger.
    auto logger = driver::Logger::Create(*ns, dispatcher(), compat::Driver::Name());
    EXPECT_EQ(ZX_OK, logger.status_value());

    // Setup start args.
    fidl::VectorView<fdf::wire::NodeSymbol> symbols(arena, 1);
    symbols[0].Allocate(arena);
    symbols[0].set_name(arena, compat::kOps);
    symbols[0].set_address(arena, reinterpret_cast<uint64_t>(ops));

    fidl::VectorView<fdata::wire::DictionaryEntry> program_entries(arena, 1);
    program_entries[0].key.Set(arena, "compat");
    program_entries[0].value.set_str(arena, "driver/v1_test.so");
    fdata::wire::Dictionary program(arena);
    program.set_entries(arena, std::move(program_entries));

    fdf::wire::DriverStartArgs start_args(arena);
    start_args.set_url(arena, "fuchsia-pkg://fuchsia.com/driver#meta/driver.cm");
    start_args.set_symbols(arena, std::move(symbols));
    start_args.set_program(arena, std::move(program));
    start_args.set_outgoing_dir(std::move(outgoing_dir_endpoints->server));

    // Start driver.
    auto result = compat::Driver::Start(start_args, dispatcher(), std::move(node), std::move(*ns),
                                        std::move(*logger));
    EXPECT_EQ(ZX_OK, result.status_value());
    return std::move(result.value());
  }

 private:
  TestNode node_;
  TestRootResource root_resource_;
  TestItems items_;
  TestDevice test_device_;
  TestFile compat_file_;
  TestFile v1_test_file_;
  TestFile firmware_file_;
  TestDirectory pkg_directory_;
  TestDirectory svc_directory_;
  TestDirectory compat_service_directory_;
};

TEST_F(DriverTest, Start) {
  zx_protocol_device_t ops{
      .get_protocol = [](void*, uint32_t, void*) { return ZX_OK; },
  };
  auto driver = StartDriver("/pkg/driver/v1_test.so", &ops);

  // Verify that v1_test.so has added a child device.
  EXPECT_FALSE(node().HasChildren());
  ASSERT_TRUE(RunLoopUntilIdle());
  EXPECT_TRUE(node().HasChildren());

  // Verify that v1_test.so has set a context.
  std::unique_ptr<V1Test> v1_test(static_cast<V1Test*>(driver->Context()));
  ASSERT_NE(nullptr, v1_test.get());

  // Verify v1_test.so state after bind.
  EXPECT_EQ(ZX_OK, v1_test->status);
  EXPECT_TRUE(v1_test->did_bind);
  EXPECT_FALSE(v1_test->did_create);
  EXPECT_FALSE(v1_test->did_release);

  // Verify v1_test.so state after release.
  driver.reset();
  ASSERT_TRUE(RunLoopUntilIdle());
  EXPECT_TRUE(v1_test->did_release);
}

TEST_F(DriverTest, Start_WithCreate) {
  zx_protocol_device_t ops{};
  auto driver = StartDriver("/pkg/driver/v1_create_test.so", &ops);

  // Verify that v1_test.so has added a child device.
  EXPECT_FALSE(node().HasChildren());
  ASSERT_TRUE(RunLoopUntilIdle());
  EXPECT_TRUE(node().HasChildren());

  // Verify that v1_test.so has set a context.
  std::unique_ptr<V1Test> v1_test(static_cast<V1Test*>(driver->Context()));
  ASSERT_NE(nullptr, v1_test.get());

  // Verify v1_test.so state after bind.
  EXPECT_EQ(ZX_OK, v1_test->status);
  EXPECT_FALSE(v1_test->did_bind);
  EXPECT_TRUE(v1_test->did_create);
  EXPECT_FALSE(v1_test->did_release);

  // Verify v1_test.so state after release.
  driver.reset();
  ASSERT_TRUE(RunLoopUntilIdle());
  EXPECT_TRUE(v1_test->did_release);
}

TEST_F(DriverTest, Start_MissingBindAndCreate) {
  zx_protocol_device_t ops{};
  auto driver = StartDriver("/pkg/driver/v1_missing_test.so", &ops);

  // Verify that v1_test.so has not added a child device.
  EXPECT_FALSE(node().HasChildren());
  ASSERT_TRUE(RunLoopUntilIdle());
  EXPECT_FALSE(node().HasChildren());

  // Verify that v1_test.so has not set a context.
  EXPECT_EQ(nullptr, driver->Context());
}

TEST_F(DriverTest, Start_DeviceAddNull) {
  zx_protocol_device_t ops{};
  auto driver = StartDriver("/pkg/driver/v1_device_add_null_test.so", &ops);

  // Verify that v1_test.so has added a child device.
  EXPECT_FALSE(node().HasChildren());
  ASSERT_TRUE(RunLoopUntilIdle());
  EXPECT_TRUE(node().HasChildren());
}

TEST_F(DriverTest, Start_CheckCompatService) {
  zx_protocol_device_t ops{};
  auto driver = StartDriver("/pkg/driver/v1_device_add_null_test.so", &ops);

  // Verify that v1_test.so has added a child device.
  EXPECT_FALSE(node().HasChildren());
  ASSERT_TRUE(RunLoopUntilIdle());
  EXPECT_TRUE(node().HasChildren());

  // Check topological path.
  ASSERT_STREQ(driver->GetDevice().topological_path().data(), "/dev/test/my-device");

  // Check metadata.
  std::array<uint8_t, 3> expected_metadata;
  std::array<uint8_t, 3> metadata;
  size_t size = 0;

  ASSERT_EQ(driver->GetDevice().GetMetadata(1, metadata.data(), metadata.size(), &size), ZX_OK);
  ASSERT_EQ(size, 3ul);
  expected_metadata = {1, 2, 3};
  ASSERT_EQ(metadata, expected_metadata);

  ASSERT_EQ(driver->GetDevice().GetMetadata(2, metadata.data(), metadata.size(), &size), ZX_OK);
  ASSERT_EQ(size, 3ul);
  expected_metadata = {4, 5, 6};
  ASSERT_EQ(metadata, expected_metadata);
}

TEST_F(DriverTest, Start_RootResourceIsConstant) {
  // Set the root resource before the test starts.
  zx_handle_t resource;
  {
    std::scoped_lock lock(kRootResourceLock);
    ASSERT_EQ(ZX_OK, zx_event_create(0, kRootResource.reset_and_get_address()));
    resource = kRootResource.get();
  }

  zx_protocol_device_t ops{};
  auto driver = StartDriver("/pkg/driver/v1_device_add_null_test.so", &ops);

  ASSERT_TRUE(RunLoopUntilIdle());

  zx_handle_t resource2 = get_root_resource();

  // Check that the root resource's value did not change.
  ASSERT_EQ(resource, resource2);
}

TEST_F(DriverTest, Start_GetBufferFailed) {
  compat_file().SetStatus(ZX_ERR_UNAVAILABLE);

  zx_protocol_device_t ops{};
  auto driver = StartDriver("/pkg/driver/v1_test.so", &ops);

  // Verify that v1_test.so has not added a child device.
  EXPECT_FALSE(node().HasChildren());
  ASSERT_TRUE(RunLoopUntilIdle());
  EXPECT_FALSE(node().HasChildren());

  // Verify that v1_test.so has not set a context.
  EXPECT_EQ(nullptr, driver->Context());
}

TEST_F(DriverTest, Start_BindFailed) {
  zx_protocol_device_t ops{};
  auto driver = StartDriver("/pkg/driver/v1_test.so", &ops);

  // Verify that v1_test.so has added a child device.
  EXPECT_FALSE(node().HasChildren());
  ASSERT_TRUE(RunLoopUntilIdle());
  EXPECT_FALSE(node().HasChildren());

  // Verify that v1_test.so has set a context.
  std::unique_ptr<V1Test> v1_test(static_cast<V1Test*>(driver->Context()));
  ASSERT_NE(nullptr, v1_test.get());

  // Verify v1_test.so state after bind.
  EXPECT_EQ(ZX_ERR_NOT_SUPPORTED, v1_test->status);
  EXPECT_TRUE(v1_test->did_bind);
  EXPECT_FALSE(v1_test->did_create);
  EXPECT_FALSE(v1_test->did_release);

  // Verify v1_test.so state after release.
  driver.reset();
  ASSERT_TRUE(RunLoopUntilIdle());
  EXPECT_TRUE(v1_test->did_release);
}

TEST_F(DriverTest, LoadFirwmareAsync) {
  zx_protocol_device_t ops{};
  auto driver = StartDriver("/pkg/driver/v1_test.so", &ops);

  // Verify that v1_test.so has added a child device.
  EXPECT_FALSE(node().HasChildren());
  ASSERT_TRUE(RunLoopUntilIdle());
  EXPECT_FALSE(node().HasChildren());

  // Verify that v1_test.so has set a context.
  std::unique_ptr<V1Test> v1_test(static_cast<V1Test*>(driver->Context()));
  ASSERT_NE(nullptr, v1_test.get());

  bool was_called = false;

  driver->LoadFirmwareAsync(
      nullptr, "test",
      [](void* ctx, zx_status_t status, zx_handle_t fw, size_t size) {
        ASSERT_EQ(status, ZX_OK);
        ASSERT_EQ(size, 16ul);
        zx::vmo vmo(fw);
        auto buf = std::vector<char>(size);
        vmo.read(buf.data(), 0, size);
        buf.push_back('\0');
        ASSERT_STREQ(buf.data(), "Hello, firmware!");

        *reinterpret_cast<bool*>(ctx) = true;
      },
      &was_called);
  ASSERT_TRUE(RunLoopUntilIdle());
  ASSERT_TRUE(was_called);

  driver.reset();
  ASSERT_TRUE(RunLoopUntilIdle());
}
