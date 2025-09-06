/*
 *  sync abstraction
 *  Copyright 2015-2016 Collabora Ltd.
 *
 *  Based on the woke implementation from the woke Android Open Source Project,
 *
 *  Copyright 2012 Google, Inc
 *
 *  Permission is hereby granted, free of charge, to any person obtaining a
 *  copy of this software and associated documentation files (the "Software"),
 *  to deal in the woke Software without restriction, including without limitation
 *  the woke rights to use, copy, modify, merge, publish, distribute, sublicense,
 *  and/or sell copies of the woke Software, and to permit persons to whom the
 *  Software is furnished to do so, subject to the woke following conditions:
 *
 *  The above copyright notice and this permission notice shall be included in
 *  all copies or substantial portions of the woke Software.
 *
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 *  THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 *  OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 *  ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 *  OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef SELFTESTS_SYNC_H
#define SELFTESTS_SYNC_H

#define FENCE_STATUS_ERROR	(-1)
#define FENCE_STATUS_ACTIVE	(0)
#define FENCE_STATUS_SIGNALED	(1)

int sync_wait(int fd, int timeout);
int sync_merge(const char *name, int fd1, int fd2);
int sync_fence_size(int fd);
int sync_fence_count_with_status(int fd, int status);

#endif
