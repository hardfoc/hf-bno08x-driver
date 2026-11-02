/**
 * @file test_sh2_session.cpp
 * @brief The CEVA SH-2 library keeps one global session (sh2.c: `sh2_t _sh2`, shtp.c:
 * SHTP_INSTANCES = 1). A second BNO085 object must be refused before it touches that state,
 * instead of silently resetting the first object's session.
 */
#include "test_harness.hpp"

#include "mock_bno08x.hpp"

namespace {

/// A second port type that never answers; only used where Begin() must be refused up front.
class SilentBus : public bno08x::CommInterface<SilentBus> {
public:
  BNO085Interface GetInterfaceType() noexcept {
    return BNO085Interface::SPI;
  }
  bool Open() noexcept {
    ++opens;
    return true;
  }
  void Close() noexcept {}
  int Write(const uint8_t*, uint32_t length) noexcept {
    return static_cast<int>(length);
  }
  int Read(uint8_t*, uint32_t) noexcept {
    return 0;
  }
  bool DataAvailable() noexcept {
    return false;
  }
  void Delay(uint32_t) noexcept {}
  uint32_t GetTimeUs() noexcept {
    return now += 10;
  }
  bool GpioSet(bno08x::CtrlPin, bno08x::GpioSignal) noexcept {
    return true;
  }
  int opens = 0;
  uint32_t now = 0;
};

MockBus respondingBus() {
  MockBus bus(BNO085Interface::I2C);
  bus.answerInitialize = true;
  return bus;
}

} // namespace

TEST_CASE("Begin() succeeds against the mock's SH-2 Initialize responder") {
  MockBus bus = respondingBus();
  BNO085<MockBus> imu(bus);
  REQUIRE(imu.Begin());
  CHECK(imu.GetState() == BNO085DriverState::Sh2Active);
  CHECK(BNO085<MockBus>::Sh2SessionInUse());
  imu.Close();
  CHECK(!BNO085<MockBus>::Sh2SessionInUse());
}

TEST_CASE("a second object's Begin() is refused while the first holds the SH-2 session") {
  MockBus busA = respondingBus();
  MockBus busB = respondingBus();
  BNO085<MockBus> a(busA);
  BNO085<MockBus> b(busB);
  REQUIRE(a.Begin());

  CHECK(!b.Begin());
  CHECK_EQ(b.GetLastError(), BNO08X_ERR_SH2_SESSION_IN_USE);
  CHECK(b.GetState() == BNO085DriverState::Closed);
  CHECK_EQ(busB.opens, 0); // refused before the SH-2 library or the bus were touched
  CHECK(busB.written.empty());

  // The first session is intact: it can still configure a report through its own bus.
  const auto before = busA.written.size();
  CHECK(a.EnableSensor(BNO085Sensor::RotationVector, 10));
  CHECK(busA.written.size() > before);
  CHECK(a.GetState() == BNO085DriverState::Sh2Active);
}

TEST_CASE("the SH-2 session is free again after Close() or destruction") {
  MockBus busA = respondingBus();
  MockBus busB = respondingBus();
  BNO085<MockBus> b(busB);
  {
    BNO085<MockBus> a(busA);
    REQUIRE(a.Begin());
    a.Close();
    REQUIRE(b.Begin());
    b.Close();
    REQUIRE(a.Begin());
  } // a's destructor closes its session
  CHECK(b.Begin());
}

TEST_CASE("the SH-2 guard spans driver objects of different port types") {
  MockBus busA = respondingBus();
  BNO085<MockBus> a(busA);
  REQUIRE(a.Begin());
  SilentBus other; // another CommType: a different BNO085<> instantiation, the same library
  BNO085<SilentBus> b(other);
  CHECK(!b.Begin());
  CHECK_EQ(b.GetLastError(), BNO08X_ERR_SH2_SESSION_IN_USE);
  CHECK_EQ(other.opens, 0);
}

TEST_CASE("a failed Begin() releases the SH-2 session") {
  MockBus bus(BNO085Interface::I2C);
  bus.openSucceeds = false;
  BNO085<MockBus> imu(bus);
  CHECK(!imu.Begin());
  CHECK(!BNO085<MockBus>::Sh2SessionInUse());
}

TEST_CASE("UART-RVC objects do not need the SH-2 session") {
  MockBus busA = respondingBus();
  BNO085<MockBus> a(busA);
  REQUIRE(a.Begin());
  MockBus rvcBus(BNO085Interface::UARTRVC);
  BNO085<MockBus> rvc(rvcBus);
  CHECK(rvc.BeginRvc());
}
