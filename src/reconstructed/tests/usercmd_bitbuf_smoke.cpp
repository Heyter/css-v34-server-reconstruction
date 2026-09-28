#include <cassert>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <vector>

#include "server/usercmd_codec.h"
#include "tests/usercmd_wire_test_utils.h"

namespace {

bool NearlyEqual(float a, float b, float epsilon = 0.01f) {
    return std::fabs(a - b) <= epsilon;
}

}  // namespace

int main() {
    using cssv34::build4100::UserCmd;
    using cssv34::reconstructed::DecodeUserCmdBatch;
    using cssv34::reconstructed::Md5PseudoRandom;
    using cssv34::reconstructed::test::BitWriter;
    using cssv34::reconstructed::test::MakeBitReader;
    using cssv34::reconstructed::test::WriteDeltaUserCmd;

    assert(Md5PseudoRandom(0) == 0x2d863277U);
    assert(Md5PseudoRandom(1) == 0xf70b7539U);
    assert(Md5PseudoRandom(10) == 0x4a4a5f1fU);
    assert(Md5PseudoRandom(11) == 0xdad4445cU);

    UserCmd zero;
    UserCmd first;
    first.commandNumber = 10;
    first.tickCount = 20;
    first.viewAngles = {90.0f, 180.0f, 45.0f};
    first.forwardMove = 100.0f;
    first.sideMove = -50.0f;
    first.upMove = 25.0f;
    first.buttons = 0x12345678;
    first.impulse = 7;
    first.weaponSelect = 17;
    first.weaponSubtype = 3;
    first.mouseDx = 11;
    first.mouseDy = -12;

    UserCmd second = first;
    second.commandNumber = 11;
    second.tickCount = 21;
    second.viewAngles.y = 270.0f;
    second.forwardMove = 120.0f;
    second.buttons = 0x01020304;
    second.mouseDx = -31;

    BitWriter writer;
    WriteDeltaUserCmd(writer, first, zero);
    WriteDeltaUserCmd(writer, second, first);

    auto reader = MakeBitReader(writer);
    std::vector<UserCmd> decoded;
    assert(DecodeUserCmdBatch(reader, 2, decoded));
    assert(!reader.overflow && reader.curBit == writer.BitCount());
    assert(decoded.size() == 2);

    assert(decoded[0].commandNumber == 10 && decoded[0].tickCount == 20);
    assert(NearlyEqual(decoded[0].viewAngles.x, 90.0f));
    assert(NearlyEqual(decoded[0].viewAngles.y, 180.0f));
    assert(NearlyEqual(decoded[0].viewAngles.z, 45.0f));
    assert(decoded[0].forwardMove == 100.0f && decoded[0].sideMove == -50.0f && decoded[0].upMove == 25.0f);
    assert(decoded[0].buttons == 0x12345678 && decoded[0].impulse == 7);
    assert(decoded[0].weaponSelect == 17 && decoded[0].weaponSubtype == 3);
    assert(decoded[0].mouseDx == 11 && decoded[0].mouseDy == -12);
    assert(decoded[0].randomSeed == static_cast<std::int32_t>(0x4a4a5f1fU & 0x7fffffffU));

    assert(decoded[1].commandNumber == 11 && decoded[1].tickCount == 21);
    assert(NearlyEqual(decoded[1].viewAngles.x, 90.0f));
    assert(NearlyEqual(decoded[1].viewAngles.y, 270.0f));
    assert(NearlyEqual(decoded[1].viewAngles.z, 45.0f));
    assert(decoded[1].forwardMove == 120.0f && decoded[1].sideMove == -50.0f && decoded[1].upMove == 25.0f);
    assert(decoded[1].buttons == 0x01020304 && decoded[1].impulse == 7);
    assert(decoded[1].weaponSelect == 17 && decoded[1].weaponSubtype == 3);
    assert(decoded[1].mouseDx == -31 && decoded[1].mouseDy == -12);
    assert(decoded[1].randomSeed == static_cast<std::int32_t>(0xdad4445cU & 0x7fffffffU));

    auto truncated = MakeBitReader(writer);
    truncated.dataBits = 7;
    decoded.clear();
    assert(!DecodeUserCmdBatch(truncated, 2, decoded));
    assert(truncated.overflow);

    auto tooMany = MakeBitReader(writer);
    decoded.clear();
    assert(!DecodeUserCmdBatch(tooMany, 29, decoded));
    assert(tooMany.overflow);

    std::cout << "USERCMD_BITBUF=bf_read_0x18,ReadUsercmd_delta,md5_seed,max_totalcmds_28\n";
    std::cout << "USERCMD_BITBUF_V0_8_0=PASS\n";
    return 0;
}
