/*
// Copyright (c) 2019 Intel Corporation
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
*/
#include "DataPacket.h"
#include <mfxvideo++.h>
#include <stdio.h>
#include <unistd.h>

int TestReference()
{
    uint8_t *raw = new uint8_t[310*310*3];
    VADataCleaner::getInstance().Initialize(true);
    VAData *data1 = VAData::Create(raw, 310, 310, 310, MFX_FOURCC_RGBP);
    printf("Main Thread: add reference to data %p\n", data1);

    printf("Main Thread: dec reference to data %p\n", data1);
    data1->DeRef();


    usleep(1000000);

    delete[] raw;
    printf("Return\n");

    return 0;
}

int TestExternalReference()
{
    uint8_t *raw = new uint8_t[310*310*3];
    VADataCleaner::getInstance().Initialize(true);
    VAData *data1 = VAData::Create(raw, 310, 310, 310, MFX_FOURCC_RGBP);
    int exRef = 0;
    data1->SetExternalRef(&exRef);
    printf("Main Thread: add reference 3 to data %p\n", data1);
    data1->SetRef(3);
    printf("Main Thread: ex reference is %d\n", exRef);
    usleep(1000000);

    printf("Main Thread: dec reference to data %p\n", data1);
    data1->DeRef();
    printf("Main Thread: ex reference is %d\n", exRef);
    usleep(1000000);

    printf("Main Thread: dec reference to data %p\n", data1);
    data1->DeRef();
    printf("Main Thread: ex reference is %d\n", exRef);
    usleep(1000000);

    printf("Main Thread: dec reference to data %p\n", data1);
    data1->DeRef();
    printf("Main Thread: ex reference is %d\n", exRef);

    usleep(1000000);

    delete[] raw;
    printf("Return\n");

    return 0;
}

int main()
{
    TestReference();
    TestExternalReference();
    return 0;
}
