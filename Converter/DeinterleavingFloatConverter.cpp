/*
 *  Copyright (C) 2010 Stephen F. Booth <me@sbooth.org>
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

#include <stdexcept>
#include <Accelerate/Accelerate.h>

#include "DeinterleavingFloatConverter.h"


DeinterleavingFloatConverter::DeinterleavingFloatConverter(const AudioStreamBasicDescription& sourceFormat)
{
	if(kAudioFormatLinearPCM != sourceFormat.mFormatID)
		throw std::runtime_error("Only PCM input formats are supported by DeinterleavingFloatConverter");

	if(kAudioFormatFlagIsPacked & sourceFormat.mFormatFlags && !(8 == sourceFormat.mBitsPerChannel || 16 == sourceFormat.mBitsPerChannel || 24 == sourceFormat.mBitsPerChannel || 32 == sourceFormat.mBitsPerChannel))
		throw std::runtime_error("Only 8, 16, 24, and 32 bit packed sample sizes are supported by DeinterleavingFloatConverter");

	UInt32 interleavedChannelCount = kAudioFormatFlagIsNonInterleaved & sourceFormat.mFormatFlags ? 1 : sourceFormat.mChannelsPerFrame;
	UInt32 sampleWidth = sourceFormat.mBytesPerFrame / interleavedChannelCount;

	if(!(kAudioFormatFlagIsPacked & sourceFormat.mFormatFlags) && !(1 == sampleWidth || 2 == sampleWidth || 3 == sampleWidth || 4 == sampleWidth) && !(8 == sourceFormat.mBitsPerChannel || 16 == sourceFormat.mBitsPerChannel || 24 == sourceFormat.mBitsPerChannel))
		throw std::runtime_error("Only 8, 16, and 24 bit sample sizes in 1, 2, 3, or 4 byte unpacked frame sizes are supported by DeinterleavingFloatConverter");

	mSourceFormat = sourceFormat;

	// This converter always produces 64-bit deinterleaved float output
	mDestinationFormat.mFormatID			= kAudioFormatLinearPCM;
	mDestinationFormat.mFormatFlags			= kAudioFormatFlagsNativeFloatPacked | kAudioFormatFlagIsNonInterleaved;
	
	mDestinationFormat.mSampleRate			= sourceFormat.mSampleRate;
	mDestinationFormat.mChannelsPerFrame	= sourceFormat.mChannelsPerFrame;
	mDestinationFormat.mBitsPerChannel		= 8 * sizeof(double);
	
	mDestinationFormat.mBytesPerPacket		= (mDestinationFormat.mBitsPerChannel / 8);
	mDestinationFormat.mFramesPerPacket		= 1;
	mDestinationFormat.mBytesPerFrame		= mDestinationFormat.mBytesPerPacket * mDestinationFormat.mFramesPerPacket;
	
	mDestinationFormat.mReserved			= 0;
}

DeinterleavingFloatConverter::~DeinterleavingFloatConverter()
{}

UInt32
DeinterleavingFloatConverter::Convert(const AudioBufferList *inputBuffer, AudioBufferList *outputBuffer, UInt32 frameCount)
{
	assert(NULL != inputBuffer);
	assert(NULL != outputBuffer);
	
	// Nothing to do
	if(0 == frameCount) {
		for(UInt32 outputBufferIndex = 0; outputBufferIndex < outputBuffer->mNumberBuffers; ++outputBufferIndex)
			outputBuffer->mBuffers[outputBufferIndex].mDataByteSize = 0;
		return 0;
	}

	UInt32 interleavedChannelCount = kAudioFormatFlagIsNonInterleaved & mSourceFormat.mFormatFlags ? 1 : mSourceFormat.mChannelsPerFrame;
	UInt32 sampleWidth = mSourceFormat.mBytesPerFrame / interleavedChannelCount;
	
	// Float-to-float conversion
	if(kAudioFormatFlagIsFloat & mSourceFormat.mFormatFlags) {
		switch(mSourceFormat.mBitsPerChannel) {
			case 32:	return ConvertFromFloat(inputBuffer, outputBuffer, frameCount);
			case 64:	return ConvertFromDouble(inputBuffer, outputBuffer, frameCount);
			default:	throw std::runtime_error("Unsupported floating point size");
		}
	}

	// Packed conversions
	else if(kAudioFormatFlagIsPacked & mSourceFormat.mFormatFlags) {
		switch(sampleWidth) {
			case 1:		return ConvertFromPacked8(inputBuffer, outputBuffer, frameCount);
			case 2:		return ConvertFromPacked16(inputBuffer, outputBuffer, frameCount);
			case 3:		return ConvertFromPacked24(inputBuffer, outputBuffer, frameCount);
			case 4:		return ConvertFromPacked32(inputBuffer, outputBuffer, frameCount);
			default:	throw std::runtime_error("Unsupported packed sample width");
		}
	}
	
	// High-aligned conversions
	else if(!(kAudioFormatFlagIsPacked & mSourceFormat.mFormatFlags) && kAudioFormatFlagIsAlignedHigh & mSourceFormat.mFormatFlags) {
		switch(sampleWidth) {
			case 1:		return ConvertFromHighAligned8(inputBuffer, outputBuffer, frameCount);
			case 2:		return ConvertFromHighAligned16(inputBuffer, outputBuffer, frameCount);
//			case 3:		return ConvertFromHighAligned24(inputBuffer, outputBuffer, frameCount);
			case 4:		return ConvertFromHighAligned32(inputBuffer, outputBuffer, frameCount);
			default:	throw std::runtime_error("Unsupported high-aligned sample width");
		}
	}

	// Low-aligned conversions
	else {
		switch(sampleWidth) {
			case 1:		return ConvertFromLowAligned8(inputBuffer, outputBuffer, frameCount);
			case 2:		return ConvertFromLowAligned16(inputBuffer, outputBuffer, frameCount);
//			case 3:		return ConvertFromLowAligned24(inputBuffer, outputBuffer, frameCount);
			case 4:		return ConvertFromLowAligned32(inputBuffer, outputBuffer, frameCount);
			default:	throw std::runtime_error("Unsupported low-aligned sample width");
		}
	}
	
	return 0;
}

#pragma mark Float Conversions

UInt32
DeinterleavingFloatConverter::ConvertFromFloat(const AudioBufferList *inputBuffer, AudioBufferList *outputBuffer, UInt32 frameCount)
{
	// Input is native floats
	if(kAudioFormatFlagsNativeEndian == (kAudioFormatFlagIsBigEndian & mSourceFormat.mFormatFlags)) {
		for(UInt32 inputBufferIndex = 0, outputBufferIndex = 0; inputBufferIndex < inputBuffer->mNumberBuffers; ++inputBufferIndex) {
			for(UInt32 inputChannelIndex = 0; inputChannelIndex < inputBuffer->mBuffers[inputBufferIndex].mNumberChannels; ++inputChannelIndex, ++outputBufferIndex) {
				float *input = static_cast<float *>(inputBuffer->mBuffers[inputBufferIndex].mData);
				double *output = static_cast<double *>(outputBuffer->mBuffers[outputBufferIndex].mData);
				
				vDSP_vspdp(input + inputChannelIndex, inputBuffer->mBuffers[inputBufferIndex].mNumberChannels, output, 1, frameCount);
				
				outputBuffer->mBuffers[outputBufferIndex].mDataByteSize = static_cast<UInt32>(frameCount * sizeof(double));
				outputBuffer->mBuffers[outputBufferIndex].mNumberChannels = 1;
			}
		}
	}
	// Input is swapped floats
	else {
		for(UInt32 inputBufferIndex = 0, outputBufferIndex = 0; inputBufferIndex < inputBuffer->mNumberBuffers; ++inputBufferIndex) {
			for(UInt32 inputChannelIndex = 0; inputChannelIndex < inputBuffer->mBuffers[inputBufferIndex].mNumberChannels; ++inputChannelIndex, ++outputBufferIndex) {
				unsigned int *input = static_cast<unsigned int *>(inputBuffer->mBuffers[inputBufferIndex].mData);
				double *output = static_cast<double *>(outputBuffer->mBuffers[outputBufferIndex].mData);
				
				for(UInt32 count = 0; count < frameCount; ++count) {
					*output++ = static_cast<float>(OSSwapInt32(*input));
					input += inputBuffer->mBuffers[inputBufferIndex].mNumberChannels;
				}
				
				outputBuffer->mBuffers[outputBufferIndex].mDataByteSize = static_cast<UInt32>(frameCount * sizeof(double));
				outputBuffer->mBuffers[outputBufferIndex].mNumberChannels = 1;
			}
		}
	}
	
	return frameCount;
}

UInt32
DeinterleavingFloatConverter::ConvertFromDouble(const AudioBufferList *inputBuffer, AudioBufferList *outputBuffer, UInt32 frameCount)
{
	// Input is native doubles
	if(kAudioFormatFlagsNativeEndian == (kAudioFormatFlagIsBigEndian & mSourceFormat.mFormatFlags)) {
		double zero = 0;

		for(UInt32 inputBufferIndex = 0, outputBufferIndex = 0; inputBufferIndex < inputBuffer->mNumberBuffers; ++inputBufferIndex) {
			for(UInt32 inputChannelIndex = 0; inputChannelIndex < inputBuffer->mBuffers[inputBufferIndex].mNumberChannels; ++inputChannelIndex, ++outputBufferIndex) {
				double *input = static_cast<double *>(inputBuffer->mBuffers[inputBufferIndex].mData);
				double *output = static_cast<double *>(outputBuffer->mBuffers[outputBufferIndex].mData);

				// It is faster to add 0 than it is to loop through the samples
				vDSP_vsaddD(input + inputChannelIndex, inputBuffer->mBuffers[inputBufferIndex].mNumberChannels, &zero, output, 1, frameCount);

				outputBuffer->mBuffers[outputBufferIndex].mDataByteSize = static_cast<UInt32>(frameCount * sizeof(double));
				outputBuffer->mBuffers[outputBufferIndex].mNumberChannels = 1;
			}
		}
	}
	// Input is swapped doubles
	else {
		for(UInt32 inputBufferIndex = 0, outputBufferIndex = 0; inputBufferIndex < inputBuffer->mNumberBuffers; ++inputBufferIndex) {
			for(UInt32 inputChannelIndex = 0; inputChannelIndex < inputBuffer->mBuffers[inputBufferIndex].mNumberChannels; ++inputChannelIndex, ++outputBufferIndex) {
				unsigned long long *input = static_cast<unsigned long long *>(inputBuffer->mBuffers[inputBufferIndex].mData);
				double *output = static_cast<double *>(outputBuffer->mBuffers[outputBufferIndex].mData);
				
				for(UInt32 count = 0; count < frameCount; ++count) {
					*output++ = static_cast<double>(OSSwapInt64(*input));
					input += inputBuffer->mBuffers[inputBufferIndex].mNumberChannels;
				}
				
				outputBuffer->mBuffers[outputBufferIndex].mDataByteSize = static_cast<UInt32>(frameCount * sizeof(double));
				outputBuffer->mBuffers[outputBufferIndex].mNumberChannels = 1;
			}
		}
	}
	
	return frameCount;
}

#pragma mark Packed Conversions

UInt32
DeinterleavingFloatConverter::ConvertFromPacked8(const AudioBufferList *inputBuffer, AudioBufferList *outputBuffer, UInt32 frameCount)
{
	double maxSignedSampleValue = 1 << 7;
	double unsignedSampleDelta = -maxSignedSampleValue;
	
	// Input is signed bytes
	if(kAudioFormatFlagIsSignedInteger & mSourceFormat.mFormatFlags) {
		for(UInt32 inputBufferIndex = 0, outputBufferIndex = 0; inputBufferIndex < inputBuffer->mNumberBuffers; ++inputBufferIndex) {
			for(UInt32 inputChannelIndex = 0; inputChannelIndex < inputBuffer->mBuffers[inputBufferIndex].mNumberChannels; ++inputChannelIndex, ++outputBufferIndex) {
				char *input = static_cast<char *>(inputBuffer->mBuffers[inputBufferIndex].mData);
				double *output = static_cast<double *>(outputBuffer->mBuffers[outputBufferIndex].mData);

				vDSP_vflt8D(input + inputChannelIndex, inputBuffer->mBuffers[inputBufferIndex].mNumberChannels, output, 1, frameCount);
				vDSP_vsdivD(output, 1, &maxSignedSampleValue, output, 1, frameCount);

				outputBuffer->mBuffers[outputBufferIndex].mDataByteSize = static_cast<UInt32>(frameCount * sizeof(double));
				outputBuffer->mBuffers[outputBufferIndex].mNumberChannels = 1;
			}
		}
	}
	// Input is unsigned bytes
	else {
		for(UInt32 inputBufferIndex = 0, outputBufferIndex = 0; inputBufferIndex < inputBuffer->mNumberBuffers; ++inputBufferIndex) {
			for(UInt32 inputChannelIndex = 0; inputChannelIndex < inputBuffer->mBuffers[inputBufferIndex].mNumberChannels; ++inputChannelIndex, ++outputBufferIndex) {
				unsigned char *input = static_cast<unsigned char *>(inputBuffer->mBuffers[inputBufferIndex].mData);
				double *output = static_cast<double *>(outputBuffer->mBuffers[outputBufferIndex].mData);
				
				vDSP_vfltu8D(input + inputChannelIndex, inputBuffer->mBuffers[inputBufferIndex].mNumberChannels, output, 1, frameCount);
				vDSP_vsaddD(output, 1, &unsignedSampleDelta, output, 1, frameCount);				
				vDSP_vsdivD(output, 1, &maxSignedSampleValue, output, 1, frameCount);
				
				outputBuffer->mBuffers[outputBufferIndex].mDataByteSize = static_cast<UInt32>(frameCount * sizeof(double));
				outputBuffer->mBuffers[outputBufferIndex].mNumberChannels = 1;
			}
		}
	}

	return frameCount;
}

UInt32
DeinterleavingFloatConverter::ConvertFromPacked16(const AudioBufferList *inputBuffer, AudioBufferList *outputBuffer, UInt32 frameCount)
{
	double maxSignedSampleValue = 1 << 15;
	double unsignedSampleDelta = -maxSignedSampleValue;
	
	if(kAudioFormatFlagsNativeEndian == (kAudioFormatFlagIsBigEndian & mSourceFormat.mFormatFlags)) {
		// Input is native signed shorts
		if(kAudioFormatFlagIsSignedInteger & mSourceFormat.mFormatFlags) {
			for(UInt32 inputBufferIndex = 0, outputBufferIndex = 0; inputBufferIndex < inputBuffer->mNumberBuffers; ++inputBufferIndex) {
				for(UInt32 inputChannelIndex = 0; inputChannelIndex < inputBuffer->mBuffers[inputBufferIndex].mNumberChannels; ++inputChannelIndex, ++outputBufferIndex) {
					short *input = static_cast<short *>(inputBuffer->mBuffers[inputBufferIndex].mData);
					double *output = static_cast<double *>(outputBuffer->mBuffers[outputBufferIndex].mData);
					
					vDSP_vflt16D(input + inputChannelIndex, inputBuffer->mBuffers[inputBufferIndex].mNumberChannels, output, 1, frameCount);
					vDSP_vsdivD(output, 1, &maxSignedSampleValue, output, 1, frameCount);

					outputBuffer->mBuffers[outputBufferIndex].mDataByteSize = static_cast<UInt32>(frameCount * sizeof(double));
					outputBuffer->mBuffers[outputBufferIndex].mNumberChannels = 1;
				}
			}
		}
		// Input is native unsigned shorts
		else {
			for(UInt32 inputBufferIndex = 0, outputBufferIndex = 0; inputBufferIndex < inputBuffer->mNumberBuffers; ++inputBufferIndex) {
				for(UInt32 inputChannelIndex = 0; inputChannelIndex < inputBuffer->mBuffers[inputBufferIndex].mNumberChannels; ++inputChannelIndex, ++outputBufferIndex) {
					unsigned short *input = static_cast<unsigned short *>(inputBuffer->mBuffers[inputBufferIndex].mData);
					double *output = static_cast<double *>(outputBuffer->mBuffers[outputBufferIndex].mData);
					
					vDSP_vfltu16D(input + inputChannelIndex, inputBuffer->mBuffers[inputBufferIndex].mNumberChannels, output, 1, frameCount);
					vDSP_vsaddD(output, 1, &unsignedSampleDelta, output, 1, frameCount);
					vDSP_vsdivD(output, 1, &maxSignedSampleValue, output, 1, frameCount);

					outputBuffer->mBuffers[outputBufferIndex].mDataByteSize = static_cast<UInt32>(frameCount * sizeof(double));
					outputBuffer->mBuffers[outputBufferIndex].mNumberChannels = 1;
				}
			}
		}
	}
	else {
		// Input is swapped signed shorts
		if(kAudioFormatFlagIsSignedInteger & mSourceFormat.mFormatFlags) {
			for(UInt32 inputBufferIndex = 0, outputBufferIndex = 0; inputBufferIndex < inputBuffer->mNumberBuffers; ++inputBufferIndex) {
				for(UInt32 inputChannelIndex = 0; inputChannelIndex < inputBuffer->mBuffers[inputBufferIndex].mNumberChannels; ++inputChannelIndex, ++outputBufferIndex) {
					short *input = static_cast<short *>(inputBuffer->mBuffers[inputBufferIndex].mData) + inputChannelIndex;
					double *output = static_cast<double *>(outputBuffer->mBuffers[outputBufferIndex].mData);
					
					for(UInt32 count = 0; count < frameCount; ++count) {
						*output++ = static_cast<short>(OSSwapInt16(*input));
						input += inputBuffer->mBuffers[inputBufferIndex].mNumberChannels;
					}
					
					output = static_cast<double *>(outputBuffer->mBuffers[outputBufferIndex].mData);

					vDSP_vsdivD(output, 1, &maxSignedSampleValue, output, 1, frameCount);

					outputBuffer->mBuffers[outputBufferIndex].mDataByteSize = static_cast<UInt32>(frameCount * sizeof(double));
					outputBuffer->mBuffers[outputBufferIndex].mNumberChannels = 1;
				}
			}
		}
		// Input is swapped unsigned shorts
		else {
			for(UInt32 inputBufferIndex = 0, outputBufferIndex = 0; inputBufferIndex < inputBuffer->mNumberBuffers; ++inputBufferIndex) {
				for(UInt32 inputChannelIndex = 0; inputChannelIndex < inputBuffer->mBuffers[inputBufferIndex].mNumberChannels; ++inputChannelIndex, ++outputBufferIndex) {
					unsigned short *input = static_cast<unsigned short *>(inputBuffer->mBuffers[inputBufferIndex].mData) + inputChannelIndex;
					double *output = static_cast<double *>(outputBuffer->mBuffers[outputBufferIndex].mData);
					
					for(UInt32 count = 0; count < frameCount; ++count) {
						*output++ = static_cast<unsigned short>(OSSwapInt16(*input));
						input += inputBuffer->mBuffers[inputBufferIndex].mNumberChannels;
					}

					output = static_cast<double *>(outputBuffer->mBuffers[outputBufferIndex].mData);

					vDSP_vsaddD(output, 1, &unsignedSampleDelta, output, 1, frameCount);
					vDSP_vsdivD(output, 1, &maxSignedSampleValue, output, 1, frameCount);

					outputBuffer->mBuffers[outputBufferIndex].mDataByteSize = static_cast<UInt32>(frameCount * sizeof(double));
					outputBuffer->mBuffers[outputBufferIndex].mNumberChannels = 1;
				}
			}
		}
	}
	
	return frameCount;
}

UInt32
DeinterleavingFloatConverter::ConvertFromPacked24(const AudioBufferList *inputBuffer, AudioBufferList *outputBuffer, UInt32 frameCount)
{
	double maxSignedSampleValue = 1 << 23;
	double unsignedSampleDelta = -maxSignedSampleValue;
	double specialNormFactor = 1 << 8;

	if(kAudioFormatFlagIsBigEndian & mSourceFormat.mFormatFlags) {
		if(kAudioFormatFlagIsSignedInteger & mSourceFormat.mFormatFlags) {
			for(UInt32 inputBufferIndex = 0, outputBufferIndex = 0; inputBufferIndex < inputBuffer->mNumberBuffers; ++inputBufferIndex) {
				for(UInt32 inputChannelIndex = 0; inputChannelIndex < inputBuffer->mBuffers[inputBufferIndex].mNumberChannels; ++inputChannelIndex, ++outputBufferIndex) {
					unsigned char *input = static_cast<unsigned char *>(inputBuffer->mBuffers[inputBufferIndex].mData) + (3 * inputChannelIndex);
					double *output = static_cast<double *>(outputBuffer->mBuffers[outputBufferIndex].mData);
					
					for(UInt32 count = 0; count < frameCount; ++count) {
						*output++ = static_cast<int>((input[0] << 24) | (input[1] << 16) | (input[2] << 8));
						input += 3 * inputBuffer->mBuffers[inputBufferIndex].mNumberChannels;
					}

					output = static_cast<double *>(outputBuffer->mBuffers[outputBufferIndex].mData);

					vDSP_vsdivD(output, 1, &specialNormFactor, output, 1, frameCount);
					vDSP_vsdivD(output, 1, &maxSignedSampleValue, output, 1, frameCount);

					outputBuffer->mBuffers[outputBufferIndex].mDataByteSize = static_cast<UInt32>(frameCount * sizeof(double));
					outputBuffer->mBuffers[outputBufferIndex].mNumberChannels = 1;
				}
			}
		}
		else {
			for(UInt32 inputBufferIndex = 0, outputBufferIndex = 0; inputBufferIndex < inputBuffer->mNumberBuffers; ++inputBufferIndex) {
				for(UInt32 inputChannelIndex = 0; inputChannelIndex < inputBuffer->mBuffers[inputBufferIndex].mNumberChannels; ++inputChannelIndex, ++outputBufferIndex) {
					unsigned char *input = static_cast<unsigned char *>(inputBuffer->mBuffers[inputBufferIndex].mData) + (3 * inputChannelIndex);
					double *output = static_cast<double *>(outputBuffer->mBuffers[outputBufferIndex].mData);
					
					for(UInt32 count = 0; count < frameCount; ++count) {
						*output++ = static_cast<unsigned int>((input[0] << 24) | (input[1] << 16) | (input[2] << 8));
						input += 3 * inputBuffer->mBuffers[inputBufferIndex].mNumberChannels;
					}
					
					output = static_cast<double *>(outputBuffer->mBuffers[outputBufferIndex].mData);
					
					vDSP_vsdivD(output, 1, &specialNormFactor, output, 1, frameCount);
					vDSP_vsaddD(output, 1, &unsignedSampleDelta, output, 1, frameCount);
					vDSP_vsdivD(output, 1, &maxSignedSampleValue, output, 1, frameCount);
					
					outputBuffer->mBuffers[outputBufferIndex].mDataByteSize = static_cast<UInt32>(frameCount * sizeof(double));
					outputBuffer->mBuffers[outputBufferIndex].mNumberChannels = 1;
				}
			}
		}
	}
	else {
		if(kAudioFormatFlagIsSignedInteger & mSourceFormat.mFormatFlags) {
			for(UInt32 inputBufferIndex = 0, outputBufferIndex = 0; inputBufferIndex < inputBuffer->mNumberBuffers; ++inputBufferIndex) {
				for(UInt32 inputChannelIndex = 0; inputChannelIndex < inputBuffer->mBuffers[inputBufferIndex].mNumberChannels; ++inputChannelIndex, ++outputBufferIndex) {
					unsigned char *input = static_cast<unsigned char *>(inputBuffer->mBuffers[inputBufferIndex].mData) + (3 * inputChannelIndex);
					double *output = static_cast<double *>(outputBuffer->mBuffers[outputBufferIndex].mData);
					
					for(UInt32 count = 0; count < frameCount; ++count) {
						*output++ = static_cast<int>((input[2] << 24) | (input[1] << 16) | (input[0] << 8));
						input += 3 * inputBuffer->mBuffers[inputBufferIndex].mNumberChannels;
					}
					
					output = static_cast<double *>(outputBuffer->mBuffers[outputBufferIndex].mData);
					
					vDSP_vsdivD(output, 1, &specialNormFactor, output, 1, frameCount);
					vDSP_vsdivD(output, 1, &maxSignedSampleValue, output, 1, frameCount);
					
					outputBuffer->mBuffers[outputBufferIndex].mDataByteSize = static_cast<UInt32>(frameCount * sizeof(double));
					outputBuffer->mBuffers[outputBufferIndex].mNumberChannels = 1;
				}
			}
		}
		else {
			for(UInt32 inputBufferIndex = 0, outputBufferIndex = 0; inputBufferIndex < inputBuffer->mNumberBuffers; ++inputBufferIndex) {
				for(UInt32 inputChannelIndex = 0; inputChannelIndex < inputBuffer->mBuffers[inputBufferIndex].mNumberChannels; ++inputChannelIndex, ++outputBufferIndex) {
					unsigned char *input = static_cast<unsigned char *>(inputBuffer->mBuffers[inputBufferIndex].mData) + (3 * inputChannelIndex);
					double *output = static_cast<double *>(outputBuffer->mBuffers[outputBufferIndex].mData);
					
					for(UInt32 count = 0; count < frameCount; ++count) {
						*output++ = static_cast<unsigned int>((input[2] << 24) | (input[1] << 16) | (input[0] << 8));
						input += 3 * inputBuffer->mBuffers[inputBufferIndex].mNumberChannels;
					}
					
					output = static_cast<double *>(outputBuffer->mBuffers[outputBufferIndex].mData);
					
					vDSP_vsdivD(output, 1, &specialNormFactor, output, 1, frameCount);
					vDSP_vsaddD(output, 1, &unsignedSampleDelta, output, 1, frameCount);
					vDSP_vsdivD(output, 1, &maxSignedSampleValue, output, 1, frameCount);
					
					outputBuffer->mBuffers[outputBufferIndex].mDataByteSize = static_cast<UInt32>(frameCount * sizeof(double));
					outputBuffer->mBuffers[outputBufferIndex].mNumberChannels = 1;
				}
			}
		}
	}

	return frameCount;
}

UInt32
DeinterleavingFloatConverter::ConvertFromPacked32(const AudioBufferList *inputBuffer, AudioBufferList *outputBuffer, UInt32 frameCount)
{
	double maxSignedSampleValue = 1 << 31;
	double unsignedSampleDelta = -maxSignedSampleValue;
	
	if(kAudioFormatFlagsNativeEndian == (kAudioFormatFlagIsBigEndian & mSourceFormat.mFormatFlags)) {
		// Input is native signed ints
		if(kAudioFormatFlagIsSignedInteger & mSourceFormat.mFormatFlags) {
			for(UInt32 inputBufferIndex = 0, outputBufferIndex = 0; inputBufferIndex < inputBuffer->mNumberBuffers; ++inputBufferIndex) {
				for(UInt32 inputChannelIndex = 0; inputChannelIndex < inputBuffer->mBuffers[inputBufferIndex].mNumberChannels; ++inputChannelIndex, ++outputBufferIndex) {
					int *input = static_cast<int *>(inputBuffer->mBuffers[inputBufferIndex].mData);
					double *output = static_cast<double *>(outputBuffer->mBuffers[outputBufferIndex].mData);
					
					vDSP_vflt32D(input + inputChannelIndex, inputBuffer->mBuffers[inputBufferIndex].mNumberChannels, output, 1, frameCount);
					vDSP_vsdivD(output, 1, &maxSignedSampleValue, output, 1, frameCount);

					outputBuffer->mBuffers[outputBufferIndex].mDataByteSize = static_cast<UInt32>(frameCount * sizeof(double));
					outputBuffer->mBuffers[outputBufferIndex].mNumberChannels = 1;
				}
			}
		}
		// Input is native unsigned ints
		else {
			for(UInt32 inputBufferIndex = 0, outputBufferIndex = 0; inputBufferIndex < inputBuffer->mNumberBuffers; ++inputBufferIndex) {
				for(UInt32 inputChannelIndex = 0; inputChannelIndex < inputBuffer->mBuffers[inputBufferIndex].mNumberChannels; ++inputChannelIndex, ++outputBufferIndex) {
					unsigned int *input = static_cast<unsigned int *>(inputBuffer->mBuffers[inputBufferIndex].mData);
					double *output = static_cast<double *>(outputBuffer->mBuffers[outputBufferIndex].mData);
					
					vDSP_vfltu32D(input + inputChannelIndex, inputBuffer->mBuffers[inputBufferIndex].mNumberChannels, output, 1, frameCount);
					vDSP_vsaddD(output, 1, &unsignedSampleDelta, output, 1, frameCount);
					vDSP_vsdivD(output, 1, &maxSignedSampleValue, output, 1, frameCount);

					outputBuffer->mBuffers[outputBufferIndex].mDataByteSize = static_cast<UInt32>(frameCount * sizeof(double));
					outputBuffer->mBuffers[outputBufferIndex].mNumberChannels = 1;
				}
			}
		}
	}
	else {
		// Input is swapped signed ints
		if(kAudioFormatFlagIsSignedInteger & mSourceFormat.mFormatFlags) {
			for(UInt32 inputBufferIndex = 0, outputBufferIndex = 0; inputBufferIndex < inputBuffer->mNumberBuffers; ++inputBufferIndex) {
				for(UInt32 inputChannelIndex = 0; inputChannelIndex < inputBuffer->mBuffers[inputBufferIndex].mNumberChannels; ++inputChannelIndex, ++outputBufferIndex) {
					int *input = static_cast<int *>(inputBuffer->mBuffers[inputBufferIndex].mData) + inputChannelIndex;
					double *output = static_cast<double *>(outputBuffer->mBuffers[outputBufferIndex].mData);
					
					for(UInt32 count = 0; count < frameCount; ++count) {
						*output++ = static_cast<int>(OSSwapInt32(*input));
						input += inputBuffer->mBuffers[inputBufferIndex].mNumberChannels;
					}
					
					output = static_cast<double *>(outputBuffer->mBuffers[outputBufferIndex].mData);

					vDSP_vsdivD(output, 1, &maxSignedSampleValue, output, 1, frameCount);

					outputBuffer->mBuffers[outputBufferIndex].mDataByteSize = static_cast<UInt32>(frameCount * sizeof(double));
					outputBuffer->mBuffers[outputBufferIndex].mNumberChannels = 1;
				}
			}
		}
		// Input is swapped unsigned ints
		else {
			for(UInt32 inputBufferIndex = 0, outputBufferIndex = 0; inputBufferIndex < inputBuffer->mNumberBuffers; ++inputBufferIndex) {
				for(UInt32 inputChannelIndex = 0; inputChannelIndex < inputBuffer->mBuffers[inputBufferIndex].mNumberChannels; ++inputChannelIndex, ++outputBufferIndex) {
					unsigned int *input = static_cast<unsigned int *>(inputBuffer->mBuffers[inputBufferIndex].mData) + inputChannelIndex;
					double *output = static_cast<double *>(outputBuffer->mBuffers[outputBufferIndex].mData);
					
					for(UInt32 count = 0; count < frameCount; ++count) {
						*output++ = static_cast<unsigned int>(OSSwapInt32(*input));
						input += inputBuffer->mBuffers[inputBufferIndex].mNumberChannels;
					}
					
					output = static_cast<double *>(outputBuffer->mBuffers[outputBufferIndex].mData);

					vDSP_vsaddD(output, 1, &unsignedSampleDelta, output, 1, frameCount);
					vDSP_vsdivD(output, 1, &maxSignedSampleValue, output, 1, frameCount);

					outputBuffer->mBuffers[outputBufferIndex].mDataByteSize = static_cast<UInt32>(frameCount * sizeof(double));
					outputBuffer->mBuffers[outputBufferIndex].mNumberChannels = 1;
				}
			}
		}
	}
	
	return frameCount;
}

#pragma mark High-Aligned Conversions

UInt32 
DeinterleavingFloatConverter::ConvertFromHighAligned8(const AudioBufferList *inputBuffer, AudioBufferList *outputBuffer, UInt32 frameCount)
{
	return ConvertFromPacked8(inputBuffer, outputBuffer, frameCount);
}

UInt32 
DeinterleavingFloatConverter::ConvertFromHighAligned16(const AudioBufferList *inputBuffer, AudioBufferList *outputBuffer, UInt32 frameCount)
{
	if(kAudioFormatFlagsNativeEndian == (kAudioFormatFlagIsBigEndian & mSourceFormat.mFormatFlags)) {
		switch(mSourceFormat.mBitsPerChannel) {
			case 8:		return ConvertFromPacked16(inputBuffer, outputBuffer, frameCount);
			default:	throw std::runtime_error("Unsupported 16 bit high-aligned bit depth");
		}
	}

	return 0;
}

//UInt32 
//DeinterleavingFloatConverter::ConvertFromHighAligned24(const AudioBufferList *inputBuffer, AudioBufferList *outputBuffer, UInt32 frameCount)
//{}

UInt32 
DeinterleavingFloatConverter::ConvertFromHighAligned32(const AudioBufferList *inputBuffer, AudioBufferList *outputBuffer, UInt32 frameCount)
{
	// 24 bit special cases
	if(24 == mSourceFormat.mBitsPerChannel) {
		double maxSignedSampleValue = 1 << 23;
		double unsignedSampleDelta = -maxSignedSampleValue;
		double specialNormFactor = 1 << 8;
		
		if(kAudioFormatFlagIsBigEndian & mSourceFormat.mFormatFlags) {
			if(kAudioFormatFlagIsSignedInteger & mSourceFormat.mFormatFlags) {
				for(UInt32 inputBufferIndex = 0, outputBufferIndex = 0; inputBufferIndex < inputBuffer->mNumberBuffers; ++inputBufferIndex) {
					for(UInt32 inputChannelIndex = 0; inputChannelIndex < inputBuffer->mBuffers[inputBufferIndex].mNumberChannels; ++inputChannelIndex, ++outputBufferIndex) {
						unsigned char *input = static_cast<unsigned char *>(inputBuffer->mBuffers[inputBufferIndex].mData) + (4 * inputChannelIndex);
						double *output = static_cast<double *>(outputBuffer->mBuffers[outputBufferIndex].mData);
						
						for(UInt32 count = 0; count < frameCount; ++count) {
							*output++ = static_cast<int>((input[0] << 24) | (input[1] << 16) | (input[2] << 8));
							input += 4 * inputBuffer->mBuffers[inputBufferIndex].mNumberChannels;
						}
						
						output = static_cast<double *>(outputBuffer->mBuffers[outputBufferIndex].mData);
						
						vDSP_vsdivD(output, 1, &specialNormFactor, output, 1, frameCount);
						vDSP_vsdivD(output, 1, &maxSignedSampleValue, output, 1, frameCount);
						
						outputBuffer->mBuffers[outputBufferIndex].mDataByteSize = static_cast<UInt32>(frameCount * sizeof(double));
						outputBuffer->mBuffers[outputBufferIndex].mNumberChannels = 1;
					}
				}
			}
			else {
				for(UInt32 inputBufferIndex = 0, outputBufferIndex = 0; inputBufferIndex < inputBuffer->mNumberBuffers; ++inputBufferIndex) {
					for(UInt32 inputChannelIndex = 0; inputChannelIndex < inputBuffer->mBuffers[inputBufferIndex].mNumberChannels; ++inputChannelIndex, ++outputBufferIndex) {
						unsigned char *input = static_cast<unsigned char *>(inputBuffer->mBuffers[inputBufferIndex].mData) + (4 * inputChannelIndex);
						double *output = static_cast<double *>(outputBuffer->mBuffers[outputBufferIndex].mData);
						
						for(UInt32 count = 0; count < frameCount; ++count) {
							*output++ = static_cast<unsigned int>((input[0] << 24) | (input[1] << 16) | (input[2] << 8));
							input += 4 * inputBuffer->mBuffers[inputBufferIndex].mNumberChannels;
						}
						
						output = static_cast<double *>(outputBuffer->mBuffers[outputBufferIndex].mData);
						
						vDSP_vsdivD(output, 1, &specialNormFactor, output, 1, frameCount);
						vDSP_vsaddD(output, 1, &unsignedSampleDelta, output, 1, frameCount);
						vDSP_vsdivD(output, 1, &maxSignedSampleValue, output, 1, frameCount);
						
						outputBuffer->mBuffers[outputBufferIndex].mDataByteSize = static_cast<UInt32>(frameCount * sizeof(double));
						outputBuffer->mBuffers[outputBufferIndex].mNumberChannels = 1;
					}
				}
			}
		}
		else {
			if(kAudioFormatFlagIsSignedInteger & mSourceFormat.mFormatFlags) {
				for(UInt32 inputBufferIndex = 0, outputBufferIndex = 0; inputBufferIndex < inputBuffer->mNumberBuffers; ++inputBufferIndex) {
					for(UInt32 inputChannelIndex = 0; inputChannelIndex < inputBuffer->mBuffers[inputBufferIndex].mNumberChannels; ++inputChannelIndex, ++outputBufferIndex) {
						unsigned char *input = static_cast<unsigned char *>(inputBuffer->mBuffers[inputBufferIndex].mData) + (4 * inputChannelIndex);
						double *output = static_cast<double *>(outputBuffer->mBuffers[outputBufferIndex].mData);
						
						for(UInt32 count = 0; count < frameCount; ++count) {
							*output++ = static_cast<int>((input[2] << 24) | (input[1] << 16) | (input[0] << 8));
							input += 4 * inputBuffer->mBuffers[inputBufferIndex].mNumberChannels;
						}
						
						output = static_cast<double *>(outputBuffer->mBuffers[outputBufferIndex].mData);
						
						vDSP_vsdivD(output, 1, &specialNormFactor, output, 1, frameCount);
						vDSP_vsdivD(output, 1, &maxSignedSampleValue, output, 1, frameCount);
						
						outputBuffer->mBuffers[outputBufferIndex].mDataByteSize = static_cast<UInt32>(frameCount * sizeof(double));
						outputBuffer->mBuffers[outputBufferIndex].mNumberChannels = 1;
					}
				}
			}
			else {
				for(UInt32 inputBufferIndex = 0, outputBufferIndex = 0; inputBufferIndex < inputBuffer->mNumberBuffers; ++inputBufferIndex) {
					for(UInt32 inputChannelIndex = 0; inputChannelIndex < inputBuffer->mBuffers[inputBufferIndex].mNumberChannels; ++inputChannelIndex, ++outputBufferIndex) {
						unsigned char *input = static_cast<unsigned char *>(inputBuffer->mBuffers[inputBufferIndex].mData) + (4 * inputChannelIndex);
						double *output = static_cast<double *>(outputBuffer->mBuffers[outputBufferIndex].mData);
						
						for(UInt32 count = 0; count < frameCount; ++count) {
							*output++ = static_cast<unsigned int>((input[2] << 24) | (input[1] << 16) | (input[0] << 8));
							input += 4 * inputBuffer->mBuffers[inputBufferIndex].mNumberChannels;
						}
						
						output = static_cast<double *>(outputBuffer->mBuffers[outputBufferIndex].mData);
						
						vDSP_vsdivD(output, 1, &specialNormFactor, output, 1, frameCount);
						vDSP_vsaddD(output, 1, &unsignedSampleDelta, output, 1, frameCount);
						vDSP_vsdivD(output, 1, &maxSignedSampleValue, output, 1, frameCount);
						
						outputBuffer->mBuffers[outputBufferIndex].mDataByteSize = static_cast<UInt32>(frameCount * sizeof(double));
						outputBuffer->mBuffers[outputBufferIndex].mNumberChannels = 1;
					}
				}
			}
		}
		
		return frameCount;
	}

	// Other cases can be handled as packed
	switch(mSourceFormat.mBitsPerChannel) {
		case 8:		return ConvertFromPacked32(inputBuffer, outputBuffer, frameCount);
		case 16:	return ConvertFromPacked32(inputBuffer, outputBuffer, frameCount);
		default:	throw std::runtime_error("Unsupported 32 bit high-aligned bit depth");
	}

	return 0;
}

#pragma mark Low-Aligned Conversions

UInt32 
DeinterleavingFloatConverter::ConvertFromLowAligned8(const AudioBufferList *inputBuffer, AudioBufferList *outputBuffer, UInt32 frameCount)
{
	return ConvertFromPacked8(inputBuffer, outputBuffer, frameCount);
}

UInt32 
DeinterleavingFloatConverter::ConvertFromLowAligned16(const AudioBufferList *inputBuffer, AudioBufferList *outputBuffer, UInt32 frameCount)
{
	UInt32 shift = 16 - mSourceFormat.mBitsPerChannel;

	for(UInt32 inputBufferIndex = 0; inputBufferIndex < inputBuffer->mNumberBuffers; ++inputBufferIndex) {
		for(UInt32 inputChannelIndex = 0; inputChannelIndex < inputBuffer->mBuffers[inputBufferIndex].mNumberChannels; ++inputChannelIndex) {
			unsigned short *input = static_cast<unsigned short *>(inputBuffer->mBuffers[inputBufferIndex].mData) + inputChannelIndex;
			
			for(UInt32 count = 0; count < frameCount; ++count) {
				*input <<= shift;
				input += inputBuffer->mBuffers[inputBufferIndex].mNumberChannels;
			}
		}
	}

	return ConvertFromHighAligned16(inputBuffer, outputBuffer, frameCount);
}

//UInt32 
//DeinterleavingFloatConverter::ConvertFromLowAligned24(const AudioBufferList *inputBuffer, AudioBufferList *outputBuffer, UInt32 frameCount)
//{}

UInt32 
DeinterleavingFloatConverter::ConvertFromLowAligned32(const AudioBufferList *inputBuffer, AudioBufferList *outputBuffer, UInt32 frameCount)
{
	UInt32 shift = 32 - mSourceFormat.mBitsPerChannel;
	
	for(UInt32 inputBufferIndex = 0; inputBufferIndex < inputBuffer->mNumberBuffers; ++inputBufferIndex) {
		for(UInt32 inputChannelIndex = 0; inputChannelIndex < inputBuffer->mBuffers[inputBufferIndex].mNumberChannels; ++inputChannelIndex) {
			unsigned int *input = static_cast<unsigned int *>(inputBuffer->mBuffers[inputBufferIndex].mData) + inputChannelIndex;
			
			for(UInt32 count = 0; count < frameCount; ++count) {
				*input <<= shift;
				input += inputBuffer->mBuffers[inputBufferIndex].mNumberChannels;
			}
		}
	}
	
	return ConvertFromHighAligned32(inputBuffer, outputBuffer, frameCount);
}