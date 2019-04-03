// Aleth: Ethereum C++ client, tools and libraries.
// Copyright 2019 Aleth Authors.
// Licensed under the GNU General Public License, Version 3.

#include <libp2p/ENR.h>
#include <gtest/gtest.h>

using namespace dev;
using namespace dev::p2p;

namespace
{
bytes dummySignFunction(bytesConstRef)
{
    return {};
}
bool dummyVerifyFunction(std::map<std::string, bytes> const&, bytesConstRef, bytesConstRef)
{
    return true;
};
}  // namespace

TEST(enr, parse)
{
    // Test ENR from EIP-778
    bytes rlp = fromHex(
        "f884b8407098ad865b00a582051940cb9cf36836572411a47278783077011599ed5cd16b76f2635f4e234738f3"
        "0813a89eb9137e3e3df5266e3a1f11df72ecf1145ccb9c01826964827634826970847f00000189736563703235"
        "366b31a103ca634cae0d49acb401d8a4c6b6fe8c55b70d115bf400769cc1400f3258cd31388375647082765f");
    ENR enr = parseV4ENR(RLP{rlp});

    EXPECT_EQ(enr.signature(),
        fromHex("7098ad865b00a582051940cb9cf36836572411a47278783077011599ed5cd16b76f2635f4e234738f3"
                "0813a89eb9137e3e3df5266e3a1f11df72ecf1145ccb9c"));
    EXPECT_EQ(enr.sequenceNumber(), 1);
    auto keyValues = enr.keyValues();
    EXPECT_EQ(RLP(keyValues["id"]).toString(), "v4");
    EXPECT_EQ(RLP(keyValues["ip"]).toBytes(), fromHex("7f000001"));
    EXPECT_EQ(RLP(keyValues["secp256k1"]).toBytes(),
        fromHex("03ca634cae0d49acb401d8a4c6b6fe8c55b70d115bf400769cc1400f3258cd3138"));
    EXPECT_EQ(RLP(keyValues["udp"]).toInt<uint64_t>(), 0x765f);
}

TEST(enr, createAndParse)
{
    auto keyPair = KeyPair::create();

    ENR enr1 = createV4ENR(keyPair.secret(), bi::address::from_string("127.0.0.1"), 3322, 5544);

    RLPStream s;
    enr1.streamRLP(s);
    bytes rlp = s.out();

    ENR enr2 = parseV4ENR(RLP{rlp});

    EXPECT_EQ(enr1.signature(), enr2.signature());
    EXPECT_EQ(enr1.sequenceNumber(), enr2.sequenceNumber());
    EXPECT_EQ(enr1.keyValues(), enr2.keyValues());
}

TEST(enr, parseTooBigRlp)
{
    std::map<std::string, bytes> keyValues = {{"key", rlp(bytes(300, 'a'))}};

    ENR enr1{0, keyValues, dummySignFunction};

    RLPStream s;
    enr1.streamRLP(s);
    bytes rlp = s.out();

    EXPECT_THROW(ENR(RLP(rlp), dummyVerifyFunction), ENRIsTooBig);
}

TEST(enr, parseKeysNotSorted)
{
    std::vector<std::pair<std::string, bytes>> keyValues = {{"keyB", RLPNull}, {"keyA", RLPNull}};

    RLPStream s((keyValues.size() * 2 + 2));
    s << bytes{};  // signature
    s << 0;        // sequence number
    for (auto const keyValue : keyValues)
    {
        s << keyValue.first;
        s.appendRaw(keyValue.second);
    }
    bytes rlp = s.out();

    EXPECT_THROW(ENR(RLP(rlp), dummyVerifyFunction), ENRKeysAreNotUniqueSorted);
}

TEST(enr, parseKeysNotUnique)
{
    std::vector<std::pair<std::string, bytes>> keyValues = {{"key", RLPNull}, {"key", RLPNull}};

    RLPStream s((keyValues.size() * 2 + 2));
    s << bytes{};  // signature
    s << 0;        // sequence number
    for (auto const keyValue : keyValues)
    {
        s << keyValue.first;
        s.appendRaw(keyValue.second);
    }
    bytes rlp = s.out();

    EXPECT_THROW(ENR(RLP(rlp), dummyVerifyFunction), ENRKeysAreNotUniqueSorted);
}

TEST(enr, parseInvalidSignature)
{
    auto keyPair = KeyPair::create();

    ENR enr1 = createV4ENR(keyPair.secret(), bi::address::from_string("127.0.0.1"), 3322, 5544);

    RLPStream s;
    enr1.streamRLP(s);
    bytes rlp = s.out();

    // change one byte of a signature
    auto signatureOffset = RLP{rlp}[0].payload().data() - rlp.data();
    rlp[signatureOffset]++;

    EXPECT_THROW(parseV4ENR(RLP{rlp}), ENRSignatureIsInvalid);
}

TEST(enr, parseV4WithInvalidID)
{
    std::map<std::string, bytes> keyValues = {{"id", rlp("v5")}};

    ENR enr1{0, keyValues, dummySignFunction};

    RLPStream s;
    enr1.streamRLP(s);
    bytes rlp = s.out();

    EXPECT_THROW(parseV4ENR(RLP{rlp}), ENRSignatureIsInvalid);
}

TEST(enr, parseV4WithNoPublicKey)
{
    std::map<std::string, bytes> keyValues = {{"id", rlp("v4")}};

    ENR enr1{0, keyValues, dummySignFunction};

    RLPStream s;
    enr1.streamRLP(s);
    bytes rlp = s.out();

    EXPECT_THROW(parseV4ENR(RLP{rlp}), ENRSignatureIsInvalid);
}

TEST(enr, createV4)
{
    auto keyPair = KeyPair::create();
    ENR enr = createV4ENR(keyPair.secret(), bi::address::from_string("127.0.0.1"), 3322, 5544);

    auto keyValues = enr.keyValues();

    EXPECT_TRUE(contains(keyValues, std::string("id")));
    EXPECT_TRUE(contains(keyValues, std::string("secp256k1")));
    EXPECT_TRUE(contains(keyValues, std::string("ip")));
    EXPECT_TRUE(contains(keyValues, std::string("tcp")));
    EXPECT_TRUE(contains(keyValues, std::string("udp")));
}
