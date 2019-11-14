/*
* Copyright (c) 2019, Intel Corporation
*
* Permission is hereby granted, free of charge, to any person obtaining a
* copy of this software and associated documentation files (the "Software"),
* to deal in the Software without restriction, including without limitation
* the rights to use, copy, modify, merge, publish, distribute, sublicense,
* and/or sell copies of the Software, and to permit persons to whom the
* Software is furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included
* in all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
* OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
* THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
* OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
* ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
* OTHER DEALINGS IN THE SOFTWARE.
*/

#include "DataPacket.h"
#include <mfxvideo++.h>
#include <stdio.h>
#include <unistd.h>

int TestReference()
{
    uint8_t *raw = new uint8_t[310*310*3];
    printf("MFX_FOURCC_RGBP is 0x%x\n", MFX_FOURCC_RGBP);
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
