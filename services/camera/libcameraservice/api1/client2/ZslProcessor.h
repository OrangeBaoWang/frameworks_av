/*
 * Copyright (C) 2013 The Android Open Source Project
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

#ifndef ANDROID_SERVERS_CAMERA_CAMERA2_ZSLPROCESSOR_H
#define ANDROID_SERVERS_CAMERA_CAMERA2_ZSLPROCESSOR_H

#include <utils/Thread.h>
#include <utils/String16.h>
#include <utils/Vector.h>
#include <utils/Mutex.h>
#include <utils/Condition.h>
#include <gui/BufferItem.h>
#include <gui/BufferItemConsumer.h>
#include <camera/CameraMetadata.h>

#include "api1/client2/FrameProcessor.h"
#include "device3/Camera3ZslStream.h"

namespace android {

class Camera2Client;

namespace camera2 {

class CaptureSequencer;
struct Parameters;

/***
 * ZSL queue processing for HALv3.0 or newer
 */
class ZslProcessor :
                    public camera3::Camera3StreamBufferListener,
            virtual public Thread,
            virtual public FrameProcessor::FilteredListener {
  public:
    ZslProcessor(sp<Camera2Client> client, wp<CaptureSequencer> sequencer);
    ~ZslProcessor();

    // From FrameProcessor::FilteredListener
    virtual void onResultAvailable(const CaptureResult &result);

    /**
     ****************************************
     * ZslProcessorInterface implementation *
     ****************************************
     */

    // Update the streams by recreating them if the size/format has changed
    status_t updateStream(const Parameters &params);

    // Delete the underlying CameraDevice streams
    status_t deleteStream();

    // Get ID for use with android.request.outputStreams / inputStreams
    int getStreamId() const;

    /**
     * Submits a ZSL capture request (id = requestId)
     *
     * An appropriate ZSL buffer is selected by the closest timestamp,
     * then we push that buffer to be reprocessed by the HAL.
     * A capture request is created and submitted on behalf of the client.
     */
    status_t pushToReprocess(int32_t requestId);

    // Flush the ZSL buffer queue, freeing up all the buffers
    status_t clearZslQueue();

    void dump(int fd, const Vector<String16>& args) const;

  protected:
    /**
     **********************************************
     * Camera3StreamBufferListener implementation *
     **********************************************
     */
    typedef camera3::Camera3StreamBufferListener::BufferInfo BufferInfo;
    // Buffer was acquired by the HAL
    virtual void onBufferAcquired(const BufferInfo& bufferInfo);
    // Buffer was released by the HAL
    virtual void onBufferReleased(const BufferInfo& bufferInfo);

  private:
    static const nsecs_t kWaitDuration = 10000000; // 10 ms
    nsecs_t mLatestClearedBufferTimestamp;

    enum {
        RUNNING,
        LOCKED
    } mState;

    wp<Camera2Client> mClient;
    wp<CaptureSequencer> mSequencer;

    const int mId;

    mutable Mutex mInputMutex;

    enum {
        NO_STREAM = -1
    };

    int mZslStreamId;
    sp<camera3::Camera3ZslStream> mZslStream;

    struct ZslPair {
        BufferItem buffer;
        CameraMetadata frame;
    };

    static const int32_t kDefaultMaxPipelineDepth = 4;
    size_t mBufferQueueDepth;
    size_t mFrameListDepth;
    Vector<CameraMetadata> mFrameList;
    size_t mFrameListHead;

    ZslPair mNextPair;

    Vector<ZslPair> mZslQueue;

    CameraMetadata mLatestCapturedRequest;

    bool mHasFocuser;

    virtual bool threadLoop();

    status_t clearZslQueueLocked();

    void clearZslResultQueueLocked();

    void dumpZslQueue(int id) const;

    nsecs_t getCandidateTimestampLocked(size_t* metadataIdx) const;

    bool isFixedFocusMode(uint8_t afMode) const;

    // Update the post-processing metadata with the default still capture request template
    status_t updateRequestWithDefaultStillRequest(CameraMetadata &request) const;
};


}; //namespace camera2
}; //namespace android

#endif
