/*
 *  Copyright (C) 2014 Stephen F. Booth <me@sbooth.org>
 *  All Rights Reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are
 *  met:
 *
 *    - Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *    - Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer in the
 *      documentation and/or other materials provided with the distribution.
 *    - Neither the name of Stephen F. Booth nor the names of its
 *      contributors may be used to endorse or promote products derived
 *      from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *  HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "AsioLibWrapper.h"

#include "ASIOOutput.h"
#include "ASIOPlayer.h"
#include "AudioFormat.h"
#include "Logger.h"

namespace {

	// ========================================
	// Missing from C++11 (from http://herbsutter.com/gotw/_102/)
	template<typename T, typename... Args>
	std::unique_ptr<T> make_unique(Args&&... args)
	{
		return std::unique_ptr<T>(new T(std::forward<Args>(args)...));
	}

	// ========================================
	// Possible event queue event types
	enum eMessageQueueEvents : uint32_t {
		eMessageQueueEventStopPlayback			= 'stop',
		eMessageQueueEventASIOResetNeeded		= 'rest',
		eMessageQueueEventASIOOverload			= 'ovld'
	};

	// ========================================
	// Convert ASIOSampleType into an AudioFormat
	SFB::Audio::AudioFormat AudioFormatForASIOSampleType(ASIOSampleType sampleType)
	{
		SFB::Audio::AudioFormat result;

		switch (sampleType) {
				// 16 bit samples
			case ASIOSTInt16LSB:
			case ASIOSTInt16MSB:
				result.mFormatID			= kAudioFormatLinearPCM;
				result.mFormatFlags			= kAudioFormatFlagIsSignedInteger | kAudioFormatFlagIsNonInterleaved | kAudioFormatFlagIsPacked;
				result.mBitsPerChannel		= 16;
				result.mBytesPerPacket		= (result.mBitsPerChannel / 8);
				result.mFramesPerPacket		= 1;
				result.mBytesPerFrame		= result.mBytesPerPacket * result.mFramesPerPacket;
				break;


				// 24 bit samples
			case ASIOSTInt24LSB:
			case ASIOSTInt24MSB:
				result.mFormatID			= kAudioFormatLinearPCM;
				result.mFormatFlags			= kAudioFormatFlagIsSignedInteger | kAudioFormatFlagIsNonInterleaved | kAudioFormatFlagIsPacked;
				result.mBitsPerChannel		= 24;
				result.mBytesPerPacket		= (result.mBitsPerChannel / 8);
				result.mFramesPerPacket		= 1;
				result.mBytesPerFrame		= result.mBytesPerPacket * result.mFramesPerPacket;
				break;


				// 32 bit samples
			case ASIOSTInt32LSB:
			case ASIOSTInt32MSB:
				result.mFormatID			= kAudioFormatLinearPCM;
				result.mFormatFlags			= kAudioFormatFlagIsSignedInteger | kAudioFormatFlagIsNonInterleaved | kAudioFormatFlagIsPacked;
				result.mBitsPerChannel		= 32;
				result.mBytesPerPacket		= (result.mBitsPerChannel / 8);
				result.mFramesPerPacket		= 1;
				result.mBytesPerFrame		= result.mBytesPerPacket * result.mFramesPerPacket;
				break;


				// 32 bit float (float) samples
			case ASIOSTFloat32LSB:
			case ASIOSTFloat32MSB:
				result.mFormatID			= kAudioFormatLinearPCM;
				result.mFormatFlags			= kAudioFormatFlagIsFloat | kAudioFormatFlagIsNonInterleaved | kAudioFormatFlagIsPacked;
				result.mBitsPerChannel		= 32;
				result.mBytesPerPacket		= (result.mBitsPerChannel / 8);
				result.mFramesPerPacket		= 1;
				result.mBytesPerFrame		= result.mBytesPerPacket * result.mFramesPerPacket;
				break;


				// 64 bit float (double) samples
			case ASIOSTFloat64LSB:
			case ASIOSTFloat64MSB:
				result.mFormatID			= kAudioFormatLinearPCM;
				result.mFormatFlags			= kAudioFormatFlagIsFloat | kAudioFormatFlagIsNonInterleaved | kAudioFormatFlagIsPacked;
				result.mBitsPerChannel		= 64;
				result.mBytesPerPacket		= (result.mBitsPerChannel / 8);
				result.mFramesPerPacket		= 1;
				result.mBytesPerFrame		= result.mBytesPerPacket * result.mFramesPerPacket;
				break;


				// other bit depths aligned in 32 bits
			case ASIOSTInt32LSB16:
			case ASIOSTInt32MSB16:
				result.mFormatID			= kAudioFormatLinearPCM;
				result.mFormatFlags			= kAudioFormatFlagIsSignedInteger | kAudioFormatFlagIsNonInterleaved;
				result.mBitsPerChannel		= 16;
				result.mBytesPerPacket		= 4;
				result.mFramesPerPacket		= 1;
				result.mBytesPerFrame		= result.mBytesPerPacket * result.mFramesPerPacket;
				break;

			case ASIOSTInt32LSB18:
			case ASIOSTInt32MSB18:
				result.mFormatID			= kAudioFormatLinearPCM;
				result.mFormatFlags			= kAudioFormatFlagIsSignedInteger | kAudioFormatFlagIsNonInterleaved;
				result.mBitsPerChannel		= 18;
				result.mBytesPerPacket		= 4;
				result.mFramesPerPacket		= 1;
				result.mBytesPerFrame		= result.mBytesPerPacket * result.mFramesPerPacket;
				break;

			case ASIOSTInt32LSB20:
			case ASIOSTInt32MSB20:
				result.mFormatID			= kAudioFormatLinearPCM;
				result.mFormatFlags			= kAudioFormatFlagIsSignedInteger | kAudioFormatFlagIsNonInterleaved;
				result.mBitsPerChannel		= 20;
				result.mBytesPerPacket		= 4;
				result.mFramesPerPacket		= 1;
				result.mBytesPerFrame		= result.mBytesPerPacket * result.mFramesPerPacket;
				break;

			case ASIOSTInt32LSB24:
			case ASIOSTInt32MSB24:
				result.mFormatID			= kAudioFormatLinearPCM;
				result.mFormatFlags			= kAudioFormatFlagIsSignedInteger | kAudioFormatFlagIsNonInterleaved;
				result.mBitsPerChannel		= 24;
				result.mBytesPerPacket		= 4;
				result.mFramesPerPacket		= 1;
				result.mBytesPerFrame		= result.mBytesPerPacket * result.mFramesPerPacket;
				break;


				// DSD
			case ASIOSTDSDInt8LSB1:
			case ASIOSTDSDInt8MSB1:
				result.mFormatID			= SFB::Audio::kAudioFormatDirectStreamDigital;
				result.mFormatFlags			= kAudioFormatFlagIsNonInterleaved;
				result.mBitsPerChannel		= 1;
				result.mBytesPerPacket		= 1;
				result.mFramesPerPacket		= 8;
				result.mBytesPerFrame		= 0;
				break;


			case ASIOSTDSDInt8NER8:
				result.mFormatID			= SFB::Audio::kAudioFormatDirectStreamDigital;
				result.mFormatFlags			= kAudioFormatFlagIsNonInterleaved;
				result.mBitsPerChannel		= 8;
				result.mBytesPerPacket		= 1;
				result.mFramesPerPacket		= 1;
				result.mBytesPerFrame		= 1;
				break;
		}


		// Add big endian flag
		switch (sampleType) {
			case ASIOSTInt16MSB:
			case ASIOSTInt24MSB:
			case ASIOSTInt32MSB:
			case ASIOSTFloat32MSB:
			case ASIOSTFloat64MSB:
			case ASIOSTInt32MSB16:
			case ASIOSTInt32MSB18:
			case ASIOSTInt32MSB20:
			case ASIOSTInt32MSB24:
			case ASIOSTDSDInt8MSB1:
				result.mFormatFlags			|= kAudioFormatFlagIsBigEndian;
				break;
		}


		return result;
	}

	// ========================================
	// Information about an ASIO driver
	struct DriverInfo
	{
		ASIODriverInfo	mDriverInfo;

		long			mInputChannelCount;
		long			mOutputChannelCount;

		long			mMinimumBufferSize;
		long			mMaximumBufferSize;
		long			mPreferredBufferSize;
		long			mBufferGranularity;

		ASIOSampleType	mFormat;
		ASIOSampleRate	mSampleRate;

		bool			mPostOutput;

		long			mInputLatency;
		long			mOutputLatency;

		long			mInputBufferCount;	// becomes number of actual created input buffers
		long			mOutputBufferCount;	// becomes number of actual created output buffers

		ASIOBufferInfo	*mBufferInfo;
		ASIOChannelInfo	*mChannelInfo;
		// The above two arrays share the same indexing, as the data in them are linked together

		AudioBufferList *mBufferList;

		// Information from ASIOGetSamplePosition()
		// data is converted to double floats for easier use, however 64 bit integer can be used, too
		double			mNanoseconds;
		double			mSamples;
		double			mTCSamples;	// time code samples

		ASIOTime		mTInfo;			// time info state
		unsigned long	mSysRefTime;      // system reference time, when bufferSwitch() was called
	};

	// ========================================
	// Callback prototypes
	void myASIOBufferSwitch(long doubleBufferIndex, ASIOBool directProcess);
	void myASIOSampleRateDidChange(ASIOSampleRate sRate);
	long myASIOMessage(long selector, long value, void *message, double *opt);
	ASIOTime * myASIOBufferSwitchTimeInfo(ASIOTime *params, long doubleBufferIndex, ASIOBool directProcess);

	// ========================================
	// Sadly ASIO requires global state
	static SFB::Audio::ASIOOutput *sOutput	= nullptr;
	static AsioDriver		*sASIO		= nullptr;
	static DriverInfo		sDriverInfo	= {{0}};
	static ASIOCallbacks	sCallbacks	= {
		.bufferSwitch			= myASIOBufferSwitch,
		.sampleRateDidChange	= myASIOSampleRateDidChange,
		.asioMessage			= myASIOMessage,
		.bufferSwitchTimeInfo	= myASIOBufferSwitchTimeInfo
	};

	// ========================================
	// Callbacks

	// Backdoor into myASIOBufferSwitchTimeInfo
	void myASIOBufferSwitch(long doubleBufferIndex, ASIOBool directProcess)
	{
		ASIOTime timeInfo = {{0}};

		auto result = sASIO->getSamplePosition(&timeInfo.timeInfo.samplePosition, &timeInfo.timeInfo.systemTime);
		if(ASE_OK == result)
			timeInfo.timeInfo.flags = kSystemTimeValid | kSamplePositionValid;

		myASIOBufferSwitchTimeInfo(&timeInfo, doubleBufferIndex, directProcess);
	}

	void myASIOSampleRateDidChange(ASIOSampleRate sRate)
	{
		LOGGER_INFO("org.sbooth.AudioEngine.Output.ASIO", "myASIOSampleRateDidChange: New sample rate " << sRate);
	}

	long myASIOMessage(long selector, long value, void *message, double *opt)
	{
		if(sOutput)
			return sOutput->HandleASIOMessage(selector, value, message, opt);
		return 0;
	}

	ASIOTime * myASIOBufferSwitchTimeInfo(ASIOTime *params, long doubleBufferIndex, ASIOBool directProcess)
	{
		if(sOutput)
			sOutput->FillASIOBuffer(doubleBufferIndex);
		return nullptr;
	}

	// ========================================

	void foo()
	{
		
	}
}

//SFB::Audio::ASIOOutput * SFB::Audio::ASIOOutput::GetInstance()
//{
//	static ASIOOutput *sOutput = nullptr;
//	static dispatch_once_t onceToken;
//	dispatch_once(&onceToken, ^{
//		sOutput = new ASIOOutput;
//	});
//	return sOutput;
//}

SFB::Audio::ASIOOutput::ASIOOutput()
	: mEventQueue(new SFB::RingBuffer)
{
	mEventQueue->Allocate(1024);

	// Setup the event dispatch timer
	mEventQueueTimer = dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER, 0, 0, dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_HIGH, 0));
	dispatch_source_set_timer(mEventQueueTimer, DISPATCH_TIME_NOW, NSEC_PER_SEC / 5, NSEC_PER_SEC / 3);

	dispatch_source_set_event_handler(mEventQueueTimer, ^{

		// Process player events
		while(mEventQueue->GetBytesAvailableToRead()) {
			uint32_t eventCode;
			auto bytesRead = mEventQueue->Read(&eventCode, sizeof(eventCode));
			if(bytesRead != sizeof(eventCode)) {
				LOGGER_ERR("org.sbooth.AudioEngine.Output.ASIO", "Error reading event from queue");
				break;
			}

			switch(eventCode) {
				case eMessageQueueEventStopPlayback:
					Stop();
					break;

				case eMessageQueueEventASIOResetNeeded:
					Reset();
					break;

				case eMessageQueueEventASIOOverload:
					LOGGER_INFO("org.sbooth.AudioEngine.Output.ASIO", "ASIO overload");
					break;
			}
		}


	});

	// Start the timer
	dispatch_resume(mEventQueueTimer);
}

SFB::Audio::ASIOOutput::~ASIOOutput()
{
	dispatch_release(mEventQueueTimer);
}

#pragma mark Device Management

bool SFB::Audio::ASIOOutput::GetDeviceIOFormat(DeviceIOFormat& deviceIOFormat) const
{
	ASIOIoFormat asioFormat;
	auto result = sASIO->future(kAsioGetIoFormat, &asioFormat);
	if(ASE_SUCCESS != result) {
		LOGGER_ERR("org.sbooth.AudioEngine.Output.ASIO", "Unable to get ASIO format: " << result);
		return false;
	}

	switch(asioFormat.FormatType) {
		case kASIOPCMFormat:	deviceIOFormat = DeviceIOFormat::eDeviceIOFormatPCM;	break;
		case kASIODSDFormat:	deviceIOFormat = DeviceIOFormat::eDeviceIOFormatDSD;	break;

		case kASIOFormatInvalid:
		default:
			return false;
	}

	return true;
}

bool SFB::Audio::ASIOOutput::SetDeviceIOFormat(const DeviceIOFormat& deviceIOFormat)
{
	ASIOIoFormat asioFormat = {
		.FormatType		= DeviceIOFormat::eDeviceIOFormatPCM == deviceIOFormat ? kASIOPCMFormat : kASIODSDFormat,
		.future			= {0}
	};

	auto result = sASIO->future(kAsioSetIoFormat, &asioFormat);
	if(ASE_SUCCESS != result) {
		LOGGER_ERR("org.sbooth.AudioEngine.Output.ASIO", "Unable to set ASIO format: " << result);
		return false;
	}

	return true;
}

#pragma mark -

bool SFB::Audio::ASIOOutput::_GetDeviceSampleRate(Float64& sampleRate) const
{
	auto result = sASIO->getSampleRate(&sampleRate);
	if(ASE_OK != result) {
		LOGGER_ERR("org.sbooth.AudioEngine.Output.ASIO", "Unable to get sample rate: " << result);
		return false;
	}

	return true;
}

bool SFB::Audio::ASIOOutput::_SetDeviceSampleRate(Float64 sampleRate)
{
	auto result = sASIO->canSampleRate(sampleRate);
	if(ASE_OK == result) {
		result = sASIO->setSampleRate(sampleRate);
		if(ASE_OK != result) {
			LOGGER_ERR("org.sbooth.AudioEngine.Output.ASIO", "Unable to set sample rate: " << result);
			return false;
		}
	}
	else {
		LOGGER_ERR("org.sbooth.AudioEngine.Output.ASIO", "Sample rate not supported: " << sampleRate);
		return false;
	}

	return true;
}

size_t SFB::Audio::ASIOOutput::_GetPreferredBufferSize() const
{
	return (size_t)sDriverInfo.mPreferredBufferSize;
}

#pragma mark -

bool SFB::Audio::ASIOOutput::_Open()
{
	int count = AsioLibWrapper::GetAsioLibraryList(nullptr, 0);
	if(0 == count) {
		LOGGER_CRIT("org.sbooth.AudioEngine.Output.ASIO", "Unable to load ASIO library list");
		return false;
	}

	AsioLibInfo buffer [count];
	count = AsioLibWrapper::GetAsioLibraryList(buffer, (unsigned int)count);
	if(0 == count) {
		LOGGER_CRIT("org.sbooth.AudioEngine.Output.ASIO", "Unable to load ASIO library list");
		return false;
	}

	// FIXME: Select the appropriate driver
	// Only 0 or 2 seems to work
	unsigned int libIndex = 0;

	if(!AsioLibWrapper::LoadLib(buffer[libIndex])) {
		LOGGER_CRIT("org.sbooth.AudioEngine.Output.ASIO", "Unable to load ASIO library");
		return false;
	}

	if(AsioLibWrapper::CreateInstance(buffer[libIndex].Number, &sASIO)) {
		LOGGER_CRIT("org.sbooth.AudioEngine.Output.ASIO", "Unable to instantiate ASIO driver");
		return false;
	}

	sDriverInfo.mDriverInfo = {
		.asioVersion = 2,
		.sysRef = nullptr
	};

	if(!sASIO->init(&sDriverInfo.mDriverInfo)){
		LOGGER_CRIT("org.sbooth.AudioEngine.Output.ASIO", "Unable to init ASIO driver: " << sDriverInfo.mDriverInfo.errorMessage);
		return false;
	}

	// Determine whether to post output notifications
	if(ASE_OK == sASIO->outputReady())
		sDriverInfo.mPostOutput = true;

	return true;
}

bool SFB::Audio::ASIOOutput::_Close()
{
	if(nullptr == sASIO)
		return false;

	sASIO->disposeBuffers();
	delete sASIO, sASIO = nullptr;
	sDriverInfo = {{0}};

	return true;
}

bool SFB::Audio::ASIOOutput::_Start()
{
	if(nullptr == sASIO || nullptr != sOutput)
		return false;

	auto result = sASIO->start();
	if(ASE_OK != result) {
		LOGGER_ERR("org.sbooth.AudioEngine.Output.ASIO", "start() failed: " << result);
		return false;
	}

	sOutput = this;

	return true;
}

bool SFB::Audio::ASIOOutput::_Stop()
{
	if(nullptr == sASIO)
		return false;

	auto result = sASIO->stop();
	if(ASE_OK != result) {
		LOGGER_ERR("org.sbooth.AudioEngine.Output.ASIO", "stop() failed: " << result);
		return false;
	}

	sOutput = nullptr;

	return true;
}

bool SFB::Audio::ASIOOutput::_RequestStop()
{
	uint32_t event = eMessageQueueEventStopPlayback;
	mEventQueue->Write(&event, sizeof(event));
	return true;
}

bool SFB::Audio::ASIOOutput::_IsRunning() const
{
	return nullptr != sOutput;
}

bool SFB::Audio::ASIOOutput::_Reset()
{
	if(!_Stop())
		return false;

	if(nullptr == sASIO)
		return false;

	sASIO->disposeBuffers();

	if(!sASIO->init(&sDriverInfo.mDriverInfo)){
		LOGGER_CRIT("org.sbooth.AudioEngine.ASIOPlayer", "Unable to init ASIO driver: " << sDriverInfo.mDriverInfo.errorMessage);
		return false;
	}

	if(ASE_OK == sASIO->outputReady())
		sDriverInfo.mPostOutput = true;
	
	return true;
}

bool SFB::Audio::ASIOOutput::_SetupForDecoder(const Decoder& decoder, AudioFormat& format1, ChannelLayout& channelLayout1)
{
	const AudioFormat& format = decoder.GetFormat();
	if(!format.IsPCM() && !format.IsDSD()) {
		LOGGER_ERR("org.sbooth.AudioEngine.Output.ASIO", "ASIO driver unsupported format: " << format);
		return false;
	}

	// Clean up existing state
	sASIO->disposeBuffers();

	sDriverInfo.mInputBufferCount = 0;
	sDriverInfo.mOutputBufferCount = 0;

	if(sDriverInfo.mBufferInfo)
		delete [] sDriverInfo.mBufferInfo, sDriverInfo.mBufferInfo = nullptr;

	if(sDriverInfo.mChannelInfo)
		delete [] sDriverInfo.mChannelInfo, sDriverInfo.mChannelInfo = nullptr;

	if(sDriverInfo.mBufferList)
		free(sDriverInfo.mBufferList), sDriverInfo.mBufferList = nullptr;

	// Configure the ASIO driver with the decoder's format
	ASIOIoFormat asioFormat = {
		.FormatType		= format.IsPCM() ? kASIOPCMFormat : format.IsDSD() ? kASIODSDFormat : kASIOFormatInvalid,
		.future			= {0}
	};

	auto result = sASIO->future(kAsioSetIoFormat, &asioFormat);
	if(ASE_SUCCESS != result) {
		LOGGER_ERR("org.sbooth.AudioEngine.Output.ASIO", "Unable to set ASIO format: " << result);
		return false;
	}

	// Set the sample rate if supported
	SetDeviceSampleRate(format.mSampleRate);


	// Store the ASIO driver format
	asioFormat = {0};
	result = sASIO->future(kAsioGetIoFormat, &asioFormat);
	if(ASE_SUCCESS != result) {
		LOGGER_ERR("org.sbooth.AudioEngine.Output.ASIO", "Unable to get ASIO format: " << result);
		return false;
	}

	sDriverInfo.mFormat = asioFormat.FormatType;
//	if(asioFormat.FormatType != format.streamType) {
//		return false;
//	}

	if(!GetDeviceSampleRate(sDriverInfo.mSampleRate))
		return false;


	// Query available channels
	result = sASIO->getChannels(&sDriverInfo.mInputChannelCount, &sDriverInfo.mOutputChannelCount);
	if(ASE_OK != result) {
		LOGGER_CRIT("org.sbooth.AudioEngine.Output.ASIO", "Unable to obtain ASIO channel count: " << result);
		return false;
	}

//	if(0 == sDriverInfo.mOutputChannelCount) {
//		LOGGER_CRIT("org.sbooth.AudioEngine.Output.ASIO", "No available output channels");
//		return false;
//	}

	// Get the preferred buffer size
	result = sASIO->getBufferSize(&sDriverInfo.mMinimumBufferSize, &sDriverInfo.mMaximumBufferSize, &sDriverInfo.mPreferredBufferSize, &sDriverInfo.mBufferGranularity);
	if(ASE_OK != result) {
		LOGGER_CRIT("org.sbooth.AudioEngine.Output.ASIO", "Unable to obtain ASIO buffer size: " << result);
		return false;
	}

	// Prepare ASIO buffers

	sDriverInfo.mInputBufferCount = std::min(sDriverInfo.mInputChannelCount, 0L);
	sDriverInfo.mOutputBufferCount = std::min(sDriverInfo.mOutputChannelCount, (long)format.mChannelsPerFrame);

	sDriverInfo.mBufferInfo = new ASIOBufferInfo [sDriverInfo.mInputBufferCount + sDriverInfo.mOutputBufferCount];
	sDriverInfo.mChannelInfo = new ASIOChannelInfo [sDriverInfo.mInputBufferCount + sDriverInfo.mOutputBufferCount];

	for(long channelIndex = 0; channelIndex < sDriverInfo.mInputBufferCount; ++channelIndex) {
		sDriverInfo.mBufferInfo[channelIndex].isInput = ASIOTrue;
		sDriverInfo.mBufferInfo[channelIndex].channelNum = channelIndex;
		sDriverInfo.mBufferInfo[channelIndex].buffers[0] = sDriverInfo.mBufferInfo[channelIndex].buffers[1] = nullptr;
	}

	for(long channelIndex = sDriverInfo.mInputBufferCount; channelIndex < sDriverInfo.mOutputBufferCount; ++channelIndex) {
		sDriverInfo.mBufferInfo[channelIndex].isInput = ASIOFalse;
		sDriverInfo.mBufferInfo[channelIndex].channelNum = channelIndex;
		sDriverInfo.mBufferInfo[channelIndex].buffers[0] = sDriverInfo.mBufferInfo[channelIndex].buffers[1] = nullptr;
	}

	// Create the buffers
	result = sASIO->createBuffers(sDriverInfo.mBufferInfo, sDriverInfo.mInputBufferCount + sDriverInfo.mOutputBufferCount, sDriverInfo.mPreferredBufferSize, &sCallbacks);
	if(ASE_OK != result) {
		LOGGER_CRIT("org.sbooth.AudioEngine.Output.ASIO", "Unable to create ASIO buffers: " << result);
		return false;
	}

	// Get the buffer details, sample word length, name, word clock group and activation
	for(long i = 0; i < sDriverInfo.mInputBufferCount + sDriverInfo.mOutputBufferCount; ++i) {
		sDriverInfo.mChannelInfo[i].channel = sDriverInfo.mBufferInfo[i].channelNum;
		sDriverInfo.mChannelInfo[i].isInput = sDriverInfo.mBufferInfo[i].isInput;

		result = sASIO->getChannelInfo(&sDriverInfo.mChannelInfo[i]);
		if(ASE_OK != result) {
			LOGGER_ERR("org.sbooth.AudioEngine.Output.ASIO", "Unable to get ASIO channel information: " << result);
			break;
		}
	}

	// Allocate a shell ABL to point to the ASIO buffers
	sDriverInfo.mBufferList = (AudioBufferList *)malloc(offsetof(AudioBufferList, mBuffers) + (sizeof(AudioBuffer) * (size_t)sDriverInfo.mOutputBufferCount));
	sDriverInfo.mBufferList->mNumberBuffers = (UInt32)sDriverInfo.mOutputBufferCount;


	// Get input and output latencies
	if(ASE_OK == result) {
		// Latencies often are only valid after ASIOCreateBuffers()
		//  (input latency is the age of the first sample in the currently returned audio block)
		//  (output latency is the time the first sample in the currently returned audio block requires to get to the output)
		result = sASIO->getLatencies(&sDriverInfo.mInputLatency, &sDriverInfo.mOutputLatency);
		if(ASE_OK != result)
			LOGGER_ERR("org.sbooth.AudioEngine.Output.ASIO", "Unable to get ASIO latencies: " << result);
	}


	// Set the ring buffer format to the first output channel
	// FIXME: Can each channel have a separate format?
	for(long i = 0; i < sDriverInfo.mInputBufferCount + sDriverInfo.mOutputBufferCount; ++i) {
		if(!sDriverInfo.mChannelInfo[i].isInput) {
			format1 = AudioFormatForASIOSampleType(sDriverInfo.mChannelInfo[i].type);
			format1.mSampleRate = sDriverInfo.mSampleRate;
			format1.mChannelsPerFrame = (UInt32)sDriverInfo.mOutputBufferCount;

			LOGGER_INFO("org.sbooth.AudioEngine.Output.ASIO", "Ring buffer format: " << format);
			break;
		}
	}

	// Attempt to set the output audio unit's channel map
	const ChannelLayout& channelLayout = decoder.GetChannelLayout();
	//	if(!SetOutputUnitChannelMap(channelLayout))
	//		LOGGER_ERR("org.sbooth.AudioEngine.Output.ASIO", "Unable to set output unit channel map");

	// The decoder's channel layout becomes the ring buffer's channel layout
	channelLayout1 = channelLayout;

	// Ensure the ring buffer is large enough
	if(4 * sDriverInfo.mPreferredBufferSize > mPlayer->GetRingBufferCapacity())
		mPlayer->SetRingBufferCapacity((uint32_t)(4 * sDriverInfo.mPreferredBufferSize));

	return true;
}

bool SFB::Audio::ASIOOutput::_CreateDeviceUID(CFStringRef& deviceUID) const
{
	return false;
}

bool SFB::Audio::ASIOOutput::_SetDeviceUID(CFStringRef deviceUID)
{
	return false;
}

#pragma mark Callbacks

long SFB::Audio::ASIOOutput::HandleASIOMessage(long selector, long value, void *message, double *opt)
{
	switch(selector) {
		case kAsioSelectorSupported:
			if(value == kAsioResetRequest || value == kAsioEngineVersion || value == kAsioResyncRequest || value == kAsioLatenciesChanged || value == kAsioSupportsTimeInfo || value == kAsioSupportsTimeCode || value == kAsioSupportsInputMonitor)
				return 1;
			break;

		case kAsioResetRequest:
		{
			uint32_t event = eMessageQueueEventASIOResetNeeded;
			mEventQueue->Write(&event, sizeof(event));
			return 1;
		}

		case kAsioOverload:
		{
			uint32_t event = eMessageQueueEventASIOOverload;
			mEventQueue->Write(&event, sizeof(event));
			return 1;
		}

		case kAsioResyncRequest:
		case kAsioLatenciesChanged:
		case kAsioSupportsTimeInfo:
			return 1;

		case kAsioEngineVersion:
			return 2;

	}

	return 0;
}

void SFB::Audio::ASIOOutput::FillASIOBuffer(long doubleBufferIndex)
{
	UInt32 frameCount = (UInt32)sDriverInfo.mPreferredBufferSize;

	// Point the shell ABL at the correct double buffer
	for(long bufferIndex = 0, ablIndex = 0; bufferIndex < sDriverInfo.mInputBufferCount + sDriverInfo.mOutputBufferCount; ++bufferIndex) {
		if(!sDriverInfo.mBufferInfo[bufferIndex].isInput) {
			sDriverInfo.mBufferList->mBuffers[ablIndex].mData = sDriverInfo.mBufferInfo[bufferIndex].buffers[doubleBufferIndex];
			sDriverInfo.mBufferList->mBuffers[ablIndex].mDataByteSize = (UInt32)mPlayer->GetRingBufferFormat().FrameCountToByteCount(frameCount);
			sDriverInfo.mBufferList->mBuffers[ablIndex].mNumberChannels = 1;
			++ablIndex;
		}
	}

	// Get audio from the player
	mPlayer->ProvideAudio(sDriverInfo.mBufferList, frameCount);

	// If the driver supports the ASIOOutputReady() optimization, do it here, all data are in place
	if(sDriverInfo.mPostOutput)
		sASIO->outputReady();
}
