/*
 * Copyright (C) 2008 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <fcntl.h>
#include <errno.h>
#include <math.h>
#include <poll.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/select.h>

#include <cutils/log.h>

#include "kxsd9.h"

/*****************************************************************************/

kxsd9Sensor::kxsd9Sensor()
: SensorBase(KXSD9_DEVICE_NAME, "accel"),
      mEnabled(0),
      mInputReader(32)
{
    mPendingEvent.version = sizeof(sensors_event_t);
    mPendingEvent.sensor = ID_A;
    mPendingEvent.type = SENSOR_TYPE_ACCELEROMETER;
    memset(mPendingEvent.data, 0, sizeof(mPendingEvent.data));
    mPendingEvent.acceleration.status = SENSOR_STATUS_ACCURACY_HIGH;

    struct input_absinfo absinfo;
//    int flags = 0;


        enable(0, 1);

            if (!ioctl(data_fd, EVIOCGABS(EVENT_TYPE_ACCEL_X), &absinfo)) {
                mPendingEvent.acceleration.x = absinfo.value * CONVERT_A_X;
            }
            if (!ioctl(data_fd, EVIOCGABS(EVENT_TYPE_ACCEL_Y), &absinfo)) {
                mPendingEvent.acceleration.y = absinfo.value * CONVERT_A_Y;
            }
            if (!ioctl(data_fd, EVIOCGABS(EVENT_TYPE_ACCEL_Z), &absinfo)) {
                mPendingEvent.acceleration.z = absinfo.value * CONVERT_A_Z;
            }
            if (!ioctl(data_fd, EVIOCGABS(EVENT_TYPE_ACCEL_STATUS), &absinfo)) {
                mPendingEvent.acceleration.status = uint8_t(absinfo.value & SENSOR_STATE_MASK);

    }

}

kxsd9Sensor::~kxsd9Sensor() {
    if (mEnabled) {
        enable(0, 0);
    }

}

int kxsd9Sensor::enable(int32_t handle, int en)
{
    int newState  = en ? 1 : 0;
    int err = 0;

    if(mEnabled != newState) {
        if (!mEnabled) {
            open_device();
        }

        int flags = newState;
        err = ioctl(dev_fd, KXSD9_IOC_SET_MODE, &flags);
        err = err<0 ? -errno : 0;
        LOGE_IF(err, "KXSD9_IOC_SET_MODE failed (%s)", strerror(-err));

        if (!err) {
            mEnabled = newState;
            setDelay(0, 200000000);
        }

        if (!mEnabled) {
            close_device();
        }
    }
    return err;
}

int kxsd9Sensor::setDelay(int32_t handle, int64_t ns)
{
    if (mEnabled) {
        if (ns < 0)
            return -EINVAL;

        int delay = ns / 1000000;
        if (ioctl(dev_fd, KXSD9_IOC_SET_DELAY, &delay)) {
            return -errno;
        }
    }
    return 0;
}

int kxsd9Sensor::readEvents(sensors_event_t* data, int count)
{
    if (count < 1)
        return -EINVAL;

    ssize_t n = mInputReader.fill(data_fd);
    if (n < 0)
        return n;

    int numEventReceived = 0;
    input_event const* event;

    while (count && mInputReader.readEvent(&event)) {
        int type = event->type;
        if (type == EV_ABS) {
            processEvent(event->code, event->value);
        } else if (type == EV_SYN) {
            mPendingEvent.timestamp = timevalToNano(event->time);
            *data++ = mPendingEvent;
            count--;
            numEventReceived++;
        } else {
            LOGE("kxsd9Sensor: unknown event (type=%d, code=%d)",
                    type, event->code);
        }
        mInputReader.next();
    }

    return numEventReceived;
}

void kxsd9Sensor::processEvent(int code, int value)
{
    switch (code) {
        case EVENT_TYPE_ACCEL_X:
            //mPendingEvent.acceleration.x = value * CONVERT_A_X;
            mPendingEvent.acceleration.x = -((value -2048 ) /81.9);//* CONVERT_A_X;
            break;
        case EVENT_TYPE_ACCEL_Y:
            //mPendingEvent.acceleration.y = value * CONVERT_A_Y;
            mPendingEvent.acceleration.y = -((value -2048 ) /81.9);//* CONVERT_A_X;
            break;
        case EVENT_TYPE_ACCEL_Z:
            //mPendingEvent.acceleration.z = value * CONVERT_A_Z;
            mPendingEvent.acceleration.z = -((value -2048 ) /81.9);//* CONVERT_A_X;
            break;
        case EVENT_TYPE_ACCEL_STATUS:
            mPendingEvent.acceleration.status =
                    uint8_t(value & SENSOR_STATE_MASK);
            break;
    }
}
