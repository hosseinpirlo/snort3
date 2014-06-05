/****************************************************************************
 *
** Copyright (C) 2014 Cisco and/or its affiliates. All rights reserved.
 * Copyright (C) 2003-2013 Sourcefire, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License Version 2 as
 * published by the Free Software Foundation.  You may not use, modify or
 * distribute this program under any other version of the GNU General
 * Public License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 ****************************************************************************/

//
//  @author     Tom Peters <thopeter@cisco.com>
//
//  @brief      NHttp Inspector class.
//


#include <assert.h>
#include <string.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdexcept>

#include "snort.h"
#include "stream/stream_api.h"
#include "nhttp_enum.h"
#include "nhttp_stream_splitter.h"
#include "nhttp_api.h"
#include "nhttp_inspect.h"

using namespace NHttpEnums;

THREAD_LOCAL NHttpMsgHeader* NHttpInspect::msgHead;
THREAD_LOCAL NHttpMsgBody* NHttpInspect::msgBody;
THREAD_LOCAL NHttpMsgChunkHead* NHttpInspect::msgChunkHead;
THREAD_LOCAL NHttpMsgChunkBody* NHttpInspect::msgChunkBody;
THREAD_LOCAL NHttpMsgTrailer* NHttpInspect::msgTrailer;

NHttpInspect::NHttpInspect(bool test_mode)
{
    /* &&& */ printf("Inspect constructor\n"); fflush(nullptr);
    NHttpTestInput::test_mode = test_mode;
    if (NHttpTestInput::test_mode) {
        NHttpTestInput::testInput = new NHttpTestInput(testInputFile);
    }
}

NHttpInspect::~NHttpInspect ()
{
    /* &&& */ printf("Inspect destructor\n"); fflush(nullptr);
    if (NHttpTestInput::test_mode) {
        delete NHttpTestInput::testInput;
        if (testOut) fclose(testOut);
    }
}

bool NHttpInspect::enabled ()
{
    /* &&& */ printf("Inspect enabled\n"); fflush(nullptr);
    return true;
}

bool NHttpInspect::configure (SnortConfig *)
{
    /* &&& */ printf("Inspect configure\n"); fflush(nullptr);
    return true;
}

int NHttpInspect::verify(SnortConfig*)
{
    /* &&& */ printf("Inspect verify\n"); fflush(nullptr);
    return 0; // 0 = good, -1 = bad
}

void NHttpInspect::pinit()
{
    /* &&& */ printf("Inspect pinit\n"); fflush(nullptr);
}

void NHttpInspect::pterm()
{
    /* &&& */ printf("Inspect pterm\n"); fflush(nullptr);
}

void NHttpInspect::show(SnortConfig*)
{
    LogMessage("NHttpInspect\n");
    /* &&& */ printf("show\n"); fflush(nullptr);
}

void NHttpInspect::eval (Packet* p)
{
    /* &&& */ printf("eval 1\n"); fflush(nullptr);
    // Only packets from the StreamSplitter can be processed
    if (!PacketHasPAFPayload(p)) return;
    /* &&& */ printf("eval 2\n"); fflush(nullptr);

    Flow *flow = p->flow;
    NHttpFlowData* sessionData = (NHttpFlowData*)flow->get_application_data(NHttpFlowData::nhttp_flow_id);
    assert(sessionData);

    NHttpMsgSection *msgSect = nullptr;

    if (!NHttpTestInput::test_mode) {
        switch (sessionData->sectionType) {
          case SEC_HEADER: msgSect = msgHead; break;
          case SEC_BODY: msgSect = msgBody; break;
          case SEC_CHUNKHEAD: msgSect = msgChunkHead; break;
          case SEC_CHUNKBODY: msgSect = msgChunkBody; break;
          case SEC_TRAILER: msgSect = msgTrailer; break;
          case SEC_DISCARD: return;
          default: assert(0); return;
        }
        msgSect->loadSection(p->data, p->dsize, sessionData);
    }
    else {
        uint8_t *testBuffer;
        uint16_t testLength;
        if ((testLength = NHttpTestInput::testInput->toEval(&testBuffer, testNumber)) > 0) {
            switch (sessionData->sectionType) {
              case SEC_HEADER: msgSect = msgHead; break;
              case SEC_BODY: msgSect = msgBody; break;
              case SEC_CHUNKHEAD: msgSect = msgChunkHead; break;
              case SEC_CHUNKBODY: msgSect = msgChunkBody; break;
              case SEC_TRAILER: msgSect = msgTrailer; break;
              case SEC_DISCARD: return;
              default: assert(0); return;
            }
            msgSect->loadSection(testBuffer, testLength, sessionData);
        }
        else {
            printf("Zero length test data.\n");
            return;
        }
    }
    msgSect->initSection();
    msgSect->analyze();
    msgSect->updateFlow();
    msgSect->genEvents();
    msgSect->legacyClients();

    if (!NHttpTestInput::test_mode) msgSect->printMessage(stdout);
    else {
        if (testNumber != fileTestNumber) {
            if (testOut) fclose (testOut);
            fileTestNumber = testNumber;
            char fileName[100];
            snprintf(fileName, sizeof(fileName), "%s%" PRIi64 ".txt", testOutputPrefix, testNumber);
            if ((testOut = fopen(fileName, "w+")) == nullptr) throw std::runtime_error("Cannot open test output file");
        }
        msgSect->printMessage(testOut);
    }
}




