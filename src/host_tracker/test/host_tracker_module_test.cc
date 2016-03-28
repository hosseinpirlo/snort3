//--------------------------------------------------------------------------
// Copyright (C) 2016-2016 Cisco and/or its affiliates. All rights reserved.
//
// This program is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License Version 2 as published
// by the Free Software Foundation.  You may not use, modify or distribute
// this program under any other version of the GNU General Public License.
//
// This program is distributed in the hope that it will be useful, but
// WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// General Public License for more details.
//
// You should have received a copy of the GNU General Public License along
// with this program; if not, write to the Free Software Foundation, Inc.,
// 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
//--------------------------------------------------------------------------

// host_tracker_module_test.cc author Steve Chew <stechew@cisco.com>
// unit tests for the host module APIs

#include "host_tracker/host_tracker_module.h"

#include <CppUTest/CommandLineTestRunner.h>
#include <CppUTest/TestHarness.h>

#include "host_tracker/host_cache.h"
#include "sfip/sf_ip.h"

//  Fake AddProtocolReference to avoid bringing in a ton of dependencies.
int16_t AddProtocolReference(const char* protocol)
{
    if (!strcmp("servicename", protocol))
        return 3;
    if (!strcmp("tcp", protocol))
        return 2;
    return 1;
}

//  Fake show_stats to avoid bringing in a ton of dependencies.
void show_stats(
    PegCount* , const PegInfo* , unsigned , const char* )
{
}

void show_stats( PegCount*, const PegInfo*, IndexVec&, const char*, FILE*)
{
}

#define FRAG_POLICY 33
#define STREAM_POLICY 100

sfip_t expected_addr;

TEST_GROUP(host_tracker_module)
{
    void setup()
    {
        Value ip_val("10.23.45.56");
        Value frag_val((double)FRAG_POLICY);
        Value tcp_val((double)STREAM_POLICY);
        Value name_val("servicename");
        Value proto_val("udp");
        Value port_val((double)2112);
        Parameter ip_param = { "ip", Parameter::PT_ADDR, nullptr, "0.0.0.0/32", "addr/cidr"};
        Parameter frag_param = { "frag_policy", Parameter::Parameter::PT_ENUM, "linux | bsd", nullptr, "frag policy"};
        Parameter tcp_param = { "tcp_policy", Parameter::PT_ENUM, "linux | bsd", nullptr, "tcp policy"};
        Parameter name_param = {"name", Parameter::PT_STRING, nullptr, nullptr, "name"};
        Parameter proto_param = {"proto", Parameter::PT_ENUM, "tcp | udp", "tcp", "ip proto"};
        Parameter port_param = {"port", Parameter::PT_PORT, nullptr, nullptr, "port num"};
        HostTrackerModule module;
        const PegInfo *ht_pegs = module.get_pegs();
        const PegCount *ht_stats = module.get_counts();

        CHECK(!strcmp(ht_pegs[0].name, "service adds"));
        CHECK(!strcmp(ht_pegs[1].name, "service finds"));
        CHECK(!strcmp(ht_pegs[2].name, "service removes"));
        CHECK(!ht_pegs[3].name);

        CHECK(ht_stats[0] == 0);
        CHECK(ht_stats[1] == 0);
        CHECK(ht_stats[2] == 0);

        ip_val.set(&ip_param);
        frag_val.set(&frag_param);
        tcp_val.set(&tcp_param);
        name_val.set(&name_param);
        proto_val.set(&proto_param);
        port_val.set(&port_param);

        // Change IP from string to integer representation.
        ip_param.validate(ip_val);

        // Set up the module values and add a service.
        module.begin("host_tracker", 1, nullptr);
        module.set(nullptr, ip_val, nullptr);
        module.set(nullptr, frag_val, nullptr);
        module.set(nullptr, tcp_val, nullptr);
        module.set(nullptr, name_val, nullptr);
        module.set(nullptr, proto_val, nullptr);
        module.set(nullptr, port_val, nullptr);
        module.end("host_tracker.services", 1, nullptr);
        module.end("host_tracker", 1, nullptr);

        ip_val.get_addr(expected_addr);
    }

    void teardown()
    {
        memset(&host_tracker_stats, 0, sizeof(host_tracker_stats));
        host_cache.clear();    //  Free HostTracker objects
    }
};

//  Test that HostTrackerModule variables are set correctly.
TEST(host_tracker_module, host_tracker_module_test_values)
{
    sfip_t cached_addr;

    HostIpKey host_ip_key(expected_addr.ip8);
    std::shared_ptr<HostTracker> ht;

    bool ret = host_cache.find(host_ip_key, ht);
    CHECK(ret == true);

    cached_addr = ht->get_ip_addr();
    CHECK(sfip_fast_equals_raw(&cached_addr, &expected_addr) == 1);

    Policy policy = ht->get_stream_policy();
    CHECK(policy == STREAM_POLICY + 1);

    policy = ht->get_frag_policy();
    CHECK(policy == FRAG_POLICY + 1);
}


//  Test that HostTrackerModule statistics are correct.
TEST(host_tracker_module, host_tracker_module_test_stats)
{
    HostIpKey host_ip_key(expected_addr.ip8);
    std::shared_ptr<HostTracker> ht;

    bool ret = host_cache.find(host_ip_key, ht);
    CHECK(ret == true);

    HostApplicationEntry app;
    ret = ht->find_service(1, 2112, app);
    CHECK(ret == true);
    CHECK(app.protocol == 3);
    CHECK(app.ipproto == 1); 
    CHECK(app.port == 2112);

    ret = ht->remove_service(1, 2112);
    CHECK(ret == true);

    //  Verify counts are correct.  The add was done during setup.
    CHECK(host_tracker_stats.service_adds == 1);
    CHECK(host_tracker_stats.service_finds == 1);
    CHECK(host_tracker_stats.service_removes == 1);
}

int main(int argc, char** argv)
{
    return CommandLineTestRunner::RunAllTests(argc, argv);
}

