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
#include "ThreadBlock.h"
#include <mfxvideo++.h>
#include <stdio.h>
#include <unistd.h>

class DummyThreadBlock : public VAThreadBlock
{
public:
    int Loop()
    {
        while (true)
        {
            usleep(1000000);
            printf("Heart beat from DummyThreadBlock\n");
        }
    }
};


int main()
{
    DummyThreadBlock *t = new DummyThreadBlock;

    t->Run();

    sleep(10);

    t->Stop();

    delete t;
}

