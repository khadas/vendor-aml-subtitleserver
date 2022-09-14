/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "SubtitleServiceLinux.h"


int main(int argc, char **argv) {
   std::shared_ptr<SubtitleServiceLinux> mpSubtitleService =
           std::shared_ptr<SubtitleServiceLinux>(SubtitleServiceLinux::GetInstance());
   mpSubtitleService->join();

   printf("SubtitleService EXIT!!!");
   return 0;
}

