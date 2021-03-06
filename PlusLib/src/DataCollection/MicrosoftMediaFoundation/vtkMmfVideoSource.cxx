/*=Plus=header=begin======================================================
Program: Plus
Copyright (c) Laboratory for Percutaneous Surgery. All rights reserved.
See License.txt for details.
=========================================================Plus=header=end*/ 

/*=========================================================================
The following copyright notice is applicable to parts of this file:
Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
All rights reserved.
See Copyright.txt or http://www.kitware.com/Copyright.htm for details.
Authors include: Adam Rankin
=========================================================================*/ 

#include "PixelCodec.h"
#include "PlusConfigure.h"
#include "vtkMmfVideoSource.h"
#include "vtkObjectFactory.h"
#include "vtkPlusChannel.h"
#include "vtkPlusDataSource.h"

#include "MediaFoundationVideoCaptureApi.h"
#include "FormatReader.h"

// Media foundation includes - require Microsoft Windows SDK 7.1 or later.
// Download from: http://www.microsoft.com/en-us/download/details.aspx?id=8279
#include <Mfapi.h>
#include <Mferror.h>

// Media foundation includes - require Microsoft Windows SDK 7.1 or later.
// Download from: http://www.microsoft.com/en-us/download/details.aspx?id=8279
#include <Mfidl.h>
#include <Mfreadwrite.h>

// Windows includes
#include <lmerr.h>
#include <shlwapi.h>
#include <tchar.h>
#include <windows.h>

//----------------------------------------------------------------------------

namespace
{
  const unsigned int DEFAULT_DEVICE_ID = 0;
  const int DEFAULT_FRAME_SIZE[2] = {640,480};
  const double DEFAULT_ACQUISITION_RATE=30;
  const std::string DEFAULT_PIXEL_TYPE_NAME="YUY2";
  const GUID DEFAULT_PIXEL_TYPE=MFVideoFormat_YUY2; // see http://msdn.microsoft.com/en-us/library/windows/desktop/aa370819(v=vs.85).aspx
  const std::string MF_VIDEO_FORMAT_PREFIX="MFVideoFormat_";
  const int ERRMSGBUFFERSIZE = 1024;
  template <class T> void SafeRelease(T **ppT)
  {
    if (*ppT)
    {
      (*ppT)->Release();
      *ppT = NULL;
    }
  }
}

//----------------------------------------------------------------------------
class MmfVideoSourceReader : public IMFSourceReaderCallback
{
public:

  MmfVideoSourceReader(vtkMmfVideoSource* plusDevice)
  : CaptureSource(NULL)
  , CaptureSourceReader(NULL)
  , RefCount(0)
  , PlusDevice(plusDevice)
  {
  };

  //------- IMFSourceReaderCallback functions ----------------------
  STDMETHODIMP QueryInterface(REFIID iid, void** ppv);
  STDMETHOD_(ULONG, AddRef)();
  STDMETHOD_(ULONG, Release)();
  STDMETHODIMP OnEvent(DWORD, IMFMediaEvent *);
  STDMETHODIMP OnFlush(DWORD);
  STDMETHODIMP OnReadSample(HRESULT hrStatus, DWORD dwStreamIndex, DWORD dwStreamFlags, LONGLONG llTimestamp, IMFSample *pSample);

  IMFMediaSource* CaptureSource;
  IMFSourceReader* CaptureSourceReader;

  long RefCount;
  vtkMmfVideoSource* PlusDevice;
};

//----------------------------------------------------------------------------
STDMETHODIMP MmfVideoSourceReader::QueryInterface( REFIID iid, void** ppv )
{
  static const QITAB qit[] =
  {
    QITABENT(MmfVideoSourceReader, IMFSourceReaderCallback),
    { 0 },
  };
  return QISearch(this, qit, iid, ppv);
}

//----------------------------------------------------------------------------

STDMETHODIMP_(ULONG) MmfVideoSourceReader::AddRef()
{
  LONG uCount=InterlockedIncrement(&RefCount);
  if (uCount==1)
  {
    this->PlusDevice->Register(NULL);
  }
  return uCount;
}

//----------------------------------------------------------------------------

STDMETHODIMP_(ULONG) MmfVideoSourceReader::Release()
{
  ULONG uCount = InterlockedDecrement(&RefCount);
  if (uCount == 0)
  {
    LOG_DEBUG("vtkMmfVideoSource::Release - unregister");
    this->PlusDevice->UnRegister(NULL);
  }
  return uCount;
}

//----------------------------------------------------------------------------
STDMETHODIMP MmfVideoSourceReader::OnEvent( DWORD, IMFMediaEvent * )
{
  return S_OK;
}

//----------------------------------------------------------------------------

STDMETHODIMP MmfVideoSourceReader::OnFlush( DWORD )
{
  return S_OK;
}

//----------------------------------------------------------------------------

STDMETHODIMP MmfVideoSourceReader::OnReadSample( HRESULT hrStatus, DWORD dwStreamIndex, DWORD dwStreamFlags, LONGLONG llTimestamp, IMFSample *pSample )
{
  PlusLockGuard<vtkRecursiveCriticalSection> updateMutexGuardedLock(this->PlusDevice->Mutex);

  if (!SUCCEEDED(hrStatus))
  {
    // Streaming error  
    LOG_ERROR("Source Reader error: " << std::hex << hrStatus);
    return S_FALSE;
  }

  if( !this->PlusDevice->IsRecording() )
  {
    return S_OK;
  }

  if (this->CaptureSourceReader==NULL)
  {
    return S_FALSE;
  }

  if (pSample!=NULL)
  {

    // Get the media type from the stream.
    IMFMediaType* pType=NULL;
    this->CaptureSourceReader->GetCurrentMediaType(this->PlusDevice->ActiveVideoFormat.StreamIndex, &pType);
    if (pType==NULL)
    {
      LOG_ERROR("Cannot get current media type");
    }

    // Check the image size, as it may be different from what we requested (even if setup does not give any error).
    // Mostly happens when the native resolution has a different aspect ration (e.g., 640x480 is requested but actually 640x360 is received).
    // The check has to be done here, the media type is not yet available at InternalConnect time.
    UINT32 actualWidth=0;
    UINT32 actualHeight=0;
    ::MFGetAttributeSize(pType, MF_MT_FRAME_SIZE, &actualWidth, &actualHeight);
    if (actualWidth!=this->PlusDevice->ActiveVideoFormat.FrameSize[0] ||
      actualHeight!=this->PlusDevice->ActiveVideoFormat.FrameSize[1])
    {
      LOG_ERROR("Unexpected frame size: "<<actualWidth<<"x"<<actualHeight<<" (expected: "<<this->PlusDevice->ActiveVideoFormat.FrameSize[0]<<"x"<<this->PlusDevice->ActiveVideoFormat.FrameSize[1]<<")"); 
      return S_FALSE;
    }

    // Check the pixel type, as it may be different from what we requested (even if setup does not give any error).
    // Mostly happens for larger resolutions (e.g., when requesting webcam feed at 1280x720 with YUY then we get MJPG).
    // The check has to be done here, the media type is not yet available at InternalConnect time.
    GUID videoFormat=DEFAULT_PIXEL_TYPE;
    pType->GetGUID( MF_MT_SUBTYPE, &videoFormat );
    std::wstring videoFormatWStr = MfVideoCapture::FormatReader::StringFromGUID(videoFormat);
    std::string videoFormatStr(videoFormatWStr.begin(), videoFormatWStr.end());
    if (videoFormatStr.compare(0, MF_VIDEO_FORMAT_PREFIX.size(), MF_VIDEO_FORMAT_PREFIX)==0)
    {
      // found standard prefix, remove it
      videoFormatStr.erase(0,MF_VIDEO_FORMAT_PREFIX.size());
    }

    if (videoFormatStr.compare(this->PlusDevice->ActiveVideoFormat.PixelFormatName)!=0)
    {
      LOG_ERROR("Unexpected video format: "<<videoFormatStr<<" (expected: "<<this->PlusDevice->ActiveVideoFormat.PixelFormatName<<")"); 
      return S_FALSE;
    }

    IMFMediaBuffer* aBuffer=NULL;
    DWORD bufferCount=0;
    pSample->GetBufferCount(&bufferCount);
    if( bufferCount < 1 )
    {
      LOG_ERROR("No buffer available in the sample.");
      return S_FALSE;
    }
    pSample->GetBufferByIndex(0, &aBuffer);
    BYTE* bufferData=NULL;
    DWORD maxLength=0;
    DWORD currentLength=0;

    HRESULT hr = aBuffer->Lock(&bufferData, &maxLength, &currentLength);
    if( SUCCEEDED(hr) ) 
    {
      this->PlusDevice->AddFrame(bufferData, currentLength);
      aBuffer->Unlock();
    }  
    else
    {
      LOG_ERROR("Unable to lock the buffer.");
    }
    SafeRelease(&aBuffer);
  }

  if (MF_SOURCE_READERF_ENDOFSTREAM & dwStreamFlags)
  {
    // Reached the end of the stream.
    LOG_ERROR("End of stream reached. Capture device should never reach end of stream.");
    this->PlusDevice->Disconnect();
    return S_FALSE;
    // This should never occur under normal operation.
  }

  this->CaptureSourceReader->ReadSample(this->PlusDevice->ActiveVideoFormat.StreamIndex, 0, NULL, NULL, NULL, NULL);

  return S_OK;
}

//----------------------------------------------------------------------------

vtkStandardNewMacro(vtkMmfVideoSource);

//----------------------------------------------------------------------------
vtkMmfVideoSource::vtkMmfVideoSource()
: FrameIndex(0)
, Mutex(vtkSmartPointer<vtkRecursiveCriticalSection>::New())
{
  this->MmfSourceReader = new MmfVideoSourceReader(this);
  this->RequireImageOrientationInConfiguration = true;

  this->AcquisitionRate = vtkPlusDevice::VIRTUAL_DEVICE_FRAME_RATE;

  this->RequestedVideoFormat.DeviceId=DEFAULT_DEVICE_ID;
  this->RequestedVideoFormat.StreamIndex=0;
  this->RequestedVideoFormat.FrameSize[0]=DEFAULT_FRAME_SIZE[0];
  this->RequestedVideoFormat.FrameSize[1]=DEFAULT_FRAME_SIZE[1];
  this->RequestedVideoFormat.PixelFormatName=DEFAULT_PIXEL_TYPE_NAME;

  this->ActiveVideoFormat.DeviceId=DEFAULT_DEVICE_ID;
  this->ActiveVideoFormat.FrameSize[0]=0;
  this->ActiveVideoFormat.FrameSize[1]=0;
}

//----------------------------------------------------------------------------
vtkMmfVideoSource::~vtkMmfVideoSource()
{
  delete this->MmfSourceReader;
  this->MmfSourceReader = NULL;
}


//----------------------------------------------------------------------------
void vtkMmfVideoSource::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os,indent);

  os << indent << "FrameIndex: " << (this->FrameIndex ? "On\n" : "Off\n");
}

//----------------------------------------------------------------------------
PlusStatus vtkMmfVideoSource::InternalConnect()
{
  PlusLockGuard<vtkRecursiveCriticalSection> updateMutexGuardedLock(this->Mutex);

  if (this->MmfSourceReader->RefCount!=0)
  {
    LOG_WARNING("There is a reference to this class from a previous connection");
  }

  this->ActiveVideoFormat = this->RequestedVideoFormat;

  GUID pixelFormat = DEFAULT_PIXEL_TYPE;
  if (!this->RequestedVideoFormat.PixelFormatName.empty())
  {
    std::string videoFormat = MF_VIDEO_FORMAT_PREFIX + this->RequestedVideoFormat.PixelFormatName;
    std::wstring videoFormatWStr(videoFormat.begin(), videoFormat.end());
    pixelFormat = MfVideoCapture::FormatReader::GUIDFromString(videoFormatWStr);
    if( pixelFormat == GUID_NULL)
    {
      LOG_ERROR("Cannot recognize requested pixel format: "<<this->RequestedVideoFormat.PixelFormatName<<". Defaulting to \'"<<DEFAULT_PIXEL_TYPE_NAME<<"\'.");
      pixelFormat = DEFAULT_PIXEL_TYPE;
    }
  }

  if( !MfVideoCapture::MediaFoundationVideoCaptureApi::GetInstance().SetupDevice(this->RequestedVideoFormat.DeviceId, this->RequestedVideoFormat.StreamIndex, 
    this->RequestedVideoFormat.FrameSize[0], this->RequestedVideoFormat.FrameSize[1], this->AcquisitionRate, pixelFormat) )
  {    
    LOG_WARNING("Unable to init capture device with requested details:"
      << " device ID: "<<this->RequestedVideoFormat.DeviceId<< " ("<<GetRequestedDeviceName()<<") stream " << this->RequestedVideoFormat.StreamIndex
      << ", "<<this->RequestedVideoFormat.FrameSize[0]<<"x"<<this->RequestedVideoFormat.FrameSize[1]
      << ", "<<this->AcquisitionRate<<"Hz, "<<this->ActiveVideoFormat.PixelFormatName);

    LogListOfCaptureVideoFormats(this->RequestedVideoFormat.DeviceId);

    if( !MfVideoCapture::MediaFoundationVideoCaptureApi::GetInstance().SetupDevice(DEFAULT_DEVICE_ID, 0, DEFAULT_FRAME_SIZE[0], DEFAULT_FRAME_SIZE[1], DEFAULT_ACQUISITION_RATE, DEFAULT_PIXEL_TYPE) )
    {
      LOG_ERROR("Unable to initialize capture device with default details: device ID: "<<DEFAULT_DEVICE_ID<<" ("<<GetCaptureDeviceName(DEFAULT_DEVICE_ID)<<") stream 0, "<<DEFAULT_FRAME_SIZE[0]<<"x"<<DEFAULT_FRAME_SIZE[1]<<", "<<DEFAULT_ACQUISITION_RATE<<"Hz, "<<DEFAULT_PIXEL_TYPE_NAME);
      LogListOfCaptureDevices();
      LogListOfCaptureVideoFormats(DEFAULT_DEVICE_ID);
      return PLUS_FAIL;
    }
    this->ActiveVideoFormat.DeviceId=DEFAULT_DEVICE_ID;
    this->ActiveVideoFormat.FrameSize[0] = DEFAULT_FRAME_SIZE[0];
    this->ActiveVideoFormat.FrameSize[1] = DEFAULT_FRAME_SIZE[1];
    this->ActiveVideoFormat.PixelFormatName = DEFAULT_PIXEL_TYPE_NAME;

    LOG_INFO("Backing up to connecting with default capture settings:"
      << " device ID: "<<this->ActiveVideoFormat.DeviceId<< " ("<<GetActiveDeviceName()<<")"
      << ", "<<this->ActiveVideoFormat.FrameSize[0]<<"x"<<this->ActiveVideoFormat.FrameSize[1]
    << ", "<<DEFAULT_ACQUISITION_RATE<<"Hz, "<<this->ActiveVideoFormat.PixelFormatName);
  }

  this->MmfSourceReader->CaptureSource = MfVideoCapture::MediaFoundationVideoCaptureApi::GetInstance().GetMediaSource(this->ActiveVideoFormat.DeviceId);
  if (this->MmfSourceReader->CaptureSource == NULL )
  {
    LOG_ERROR("Unable to request capture source from the media foundation library.");
    return PLUS_FAIL;
  }

  unsigned int frameRate=MfVideoCapture::MediaFoundationVideoCaptureApi::GetInstance().GetFrameRate(this->ActiveVideoFormat.DeviceId);
  LOG_DEBUG("vtkMmfVideoSource connected to device '"<<GetActiveDeviceName()<<"' at frame rate of "<<frameRate<<"Hz");

  this->FrameIndex = 0;

  return PLUS_SUCCESS;
}

//----------------------------------------------------------------------------
PlusStatus vtkMmfVideoSource::InternalDisconnect()
{
  PlusLockGuard<vtkRecursiveCriticalSection> updateMutexGuardedLock(this->Mutex);

  MfVideoCapture::MediaFoundationVideoCaptureApi::GetInstance().CloseDevice(this->ActiveVideoFormat.DeviceId);
  this->MmfSourceReader->CaptureSource = NULL;

  return PLUS_SUCCESS;
}

//----------------------------------------------------------------------------
PlusStatus vtkMmfVideoSource::InternalStartRecording()
{
  PlusLockGuard<vtkRecursiveCriticalSection> updateMutexGuardedLock(this->Mutex);

  HRESULT hr;
  if( this->MmfSourceReader->CaptureSource != NULL )
  {
    IMFAttributes* attr;
    MFCreateAttributes(&attr, 2);
    hr = attr->SetUnknown(MF_SOURCE_READER_ASYNC_CALLBACK, this->MmfSourceReader);
    hr = attr->SetUINT32(MF_READWRITE_DISABLE_CONVERTERS, TRUE);

    hr = MFCreateSourceReaderFromMediaSource(this->MmfSourceReader->CaptureSource, attr, &this->MmfSourceReader->CaptureSourceReader);

    if( FAILED(hr) )
    {
      LOG_ERROR("Unable to create source reader from media source.");
      return PLUS_FAIL;
    }
    this->UpdateFrameSize();

    attr->Release();

    std::string videoFormat = MF_VIDEO_FORMAT_PREFIX + this->RequestedVideoFormat.PixelFormatName;
    std::wstring videoFormatWStr(videoFormat.begin(), videoFormat.end());

    IMFMediaType *pDecodeType = NULL;
    hr = MFCreateMediaType(&pDecodeType);
    hr = pDecodeType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
    hr = pDecodeType->SetGUID(MF_MT_SUBTYPE, MfVideoCapture::FormatReader::GUIDFromString(videoFormatWStr));

    hr = this->MmfSourceReader->CaptureSourceReader->SetCurrentMediaType(this->ActiveVideoFormat.StreamIndex, NULL, pDecodeType);
    if ( FAILED(hr) )
    {
      LOG_WARNING("Unable to set SourceReader output to requested format: " << this->RequestedVideoFormat.PixelFormatName
                  << ". Using device default.");
    }
    SafeRelease(&pDecodeType);
    
    MfVideoCapture::MediaFoundationVideoCaptureApi::GetInstance().StartRecording(this->ActiveVideoFormat.DeviceId);
    this->MmfSourceReader->CaptureSourceReader->ReadSample(this->ActiveVideoFormat.StreamIndex, 0, NULL, NULL, NULL, NULL);
  }
  else
  {
    LOG_ERROR("Unable to request IMFMediaSource from the media foundation capture library. Unable to continue.");
    return PLUS_FAIL;
  }

  return PLUS_SUCCESS;
}

//----------------------------------------------------------------------------
PlusStatus vtkMmfVideoSource::InternalStopRecording()
{
  LOG_DEBUG("vtkMmfVideoSource::InternalStopRecording");

  PlusLockGuard<vtkRecursiveCriticalSection> updateMutexGuardedLock(this->Mutex);

  MfVideoCapture::MediaFoundationVideoCaptureApi::GetInstance().StopRecording(this->ActiveVideoFormat.DeviceId);

  this->MmfSourceReader->CaptureSourceReader->Flush(this->ActiveVideoFormat.StreamIndex);
  SafeRelease(&this->MmfSourceReader->CaptureSourceReader);
  return PLUS_SUCCESS; 
}

//----------------------------------------------------------------------------

PlusStatus vtkMmfVideoSource::NotifyConfigured()
{
  if( this->OutputChannels.size() > 1 )
  {
    LOG_WARNING("vtkMmfVideoSource is expecting one output channel and there are " << this->OutputChannels.size() << " channels. First output channel will be used.");
    return PLUS_FAIL;
  }

  if( this->OutputChannels.size() == 0 )
  {
    LOG_ERROR("No output channels defined for microsoft media foundation video source. Cannot proceed." );
    this->CorrectlyConfigured = false;
    return PLUS_FAIL;
  }

  return PLUS_SUCCESS;
}

//----------------------------------------------------------------------------
PlusStatus vtkMmfVideoSource::UpdateFrameSize()
{
  if( this->MmfSourceReader->CaptureSourceReader != NULL )
  {
    int currentFrameSize[3] = {0,0,1};
    vtkPlusDataSource* videoSource(NULL);
    this->GetFirstVideoSource(videoSource);
    videoSource->GetInputFrameSize(currentFrameSize);
    if( currentFrameSize[0] != this->ActiveVideoFormat.FrameSize[0] || currentFrameSize[1] != this->ActiveVideoFormat.FrameSize[1] || currentFrameSize[2] != 1 )
    {
      currentFrameSize[0] = this->ActiveVideoFormat.FrameSize[0];
      currentFrameSize[1] = this->ActiveVideoFormat.FrameSize[1];
      videoSource->SetInputFrameSize(currentFrameSize);
      videoSource->SetPixelType(VTK_UNSIGNED_CHAR);
      int numberOfScalarComponents = (videoSource->GetImageType() == US_IMG_RGB_COLOR ? 3 : 1);
      videoSource->SetNumberOfScalarComponents(numberOfScalarComponents);
      this->UncompressedVideoFrame.SetImageType(videoSource->GetImageType());
      this->UncompressedVideoFrame.SetImageOrientation(videoSource->GetInputImageOrientation());
      this->UncompressedVideoFrame.AllocateFrame(currentFrameSize, VTK_UNSIGNED_CHAR, numberOfScalarComponents);
    }
  }

  return PLUS_SUCCESS;
}

//----------------------------------------------------------------------------
PlusStatus vtkMmfVideoSource::ReadConfiguration( vtkXMLDataElement* rootConfigElement )
{
  XML_FIND_DEVICE_ELEMENT_REQUIRED_FOR_READING(deviceConfig, rootConfigElement);

  int deviceId=0;
  if (deviceConfig->GetScalarAttribute("CaptureDeviceId", deviceId))
  {
    this->RequestedVideoFormat.DeviceId=deviceId;
  }

  int streamIndex=0;
  if (deviceConfig->GetScalarAttribute("CaptureStreamIndex", streamIndex))
  {
    this->RequestedVideoFormat.StreamIndex=(DWORD)streamIndex;
  }

  int requestedFrameSize[2]={DEFAULT_FRAME_SIZE[0],DEFAULT_FRAME_SIZE[1]};
  if( deviceConfig->GetVectorAttribute("FrameSize", 2, requestedFrameSize) )
  {
    this->RequestedVideoFormat.FrameSize[0] = requestedFrameSize[0];
    this->RequestedVideoFormat.FrameSize[1] = requestedFrameSize[1];
  }  

  this->RequestedVideoFormat.PixelFormatName.clear();
  if( deviceConfig->GetAttribute("VideoFormat") != NULL )
  {
    this->RequestedVideoFormat.PixelFormatName=deviceConfig->GetAttribute("VideoFormat");
  }

  return PLUS_SUCCESS;
}

//----------------------------------------------------------------------------
PlusStatus vtkMmfVideoSource::WriteConfiguration( vtkXMLDataElement* rootConfigElement )
{
  XML_FIND_DEVICE_ELEMENT_REQUIRED_FOR_WRITING(deviceConfig, rootConfigElement);

  deviceConfig->SetIntAttribute("CaptureDeviceId", this->RequestedVideoFormat.DeviceId);
  if (this->RequestedVideoFormat.StreamIndex!=0)
  {
    deviceConfig->SetIntAttribute("CaptureStreamIndex", this->RequestedVideoFormat.StreamIndex);
  }
  else
  {
    XML_REMOVE_ATTRIBUTE(deviceConfig, "CaptureStreamIndex");
  }
  deviceConfig->SetVectorAttribute("FrameSize", 2, this->RequestedVideoFormat.FrameSize);
  deviceConfig->SetAttribute("VideoFormat", this->RequestedVideoFormat.PixelFormatName.c_str());

  return PLUS_SUCCESS;
}

//----------------------------------------------------------------------------
void vtkMmfVideoSource::SetRequestedVideoFormat(const std::string& pixelFormatName)
{
  this->RequestedVideoFormat.PixelFormatName = pixelFormatName;
}

void vtkMmfVideoSource::SetRequestedStreamIndex(unsigned int streamIndex)
{
  this->RequestedVideoFormat.StreamIndex = (DWORD)streamIndex;
}

//----------------------------------------------------------------------------
void vtkMmfVideoSource::SetRequestedFrameSize(int frameSize[2])
{
  this->RequestedVideoFormat.FrameSize[0] = frameSize[0];
  this->RequestedVideoFormat.FrameSize[1] = frameSize[1];
}

//----------------------------------------------------------------------------
void vtkMmfVideoSource::SetRequestedDeviceId(unsigned int deviceId)
{
  this->RequestedVideoFormat.DeviceId = deviceId;
}

//----------------------------------------------------------------------------
void vtkMmfVideoSource::GetListOfCaptureDevices(std::vector< std::string > &deviceNames)
{
  deviceNames.clear();
  std::vector< std::wstring > deviceNamesW;
  MfVideoCapture::MediaFoundationVideoCaptureApi::GetInstance().GetDeviceNames(deviceNamesW);
  for (std::vector< std::wstring > :: iterator deviceNameWIt=deviceNamesW.begin(); deviceNameWIt!=deviceNamesW.end(); ++deviceNameWIt)
  {
    std::string deviceName(deviceNameWIt->begin(), deviceNameWIt->end());
    deviceNames.push_back(deviceName);
  }
}

//----------------------------------------------------------------------------
void vtkMmfVideoSource::GetListOfCaptureVideoFormats(std::vector< std::string > &videoModes, unsigned int deviceId)
{
  videoModes.clear();
  unsigned int numberOfStreams = MfVideoCapture::MediaFoundationVideoCaptureApi::GetInstance().GetNumberOfStreams(deviceId);
  for (unsigned int streamIndex=0; streamIndex<numberOfStreams; ++streamIndex)
  {
    unsigned int numberOfVideoFormats = MfVideoCapture::MediaFoundationVideoCaptureApi::GetInstance().GetNumberOfFormats(deviceId, streamIndex);
    for (unsigned int formatIndex = 0; formatIndex < numberOfVideoFormats; formatIndex++)
    {
      MfVideoCapture::MediaType type = MfVideoCapture::MediaFoundationVideoCaptureApi::GetInstance().GetFormat(deviceId, streamIndex, formatIndex);
      std::string pixelType(type.MF_MT_SUBTYPEName.begin(), type.MF_MT_SUBTYPEName.end());
      if (pixelType.compare(0, MF_VIDEO_FORMAT_PREFIX.size(), MF_VIDEO_FORMAT_PREFIX)==0)
      {
        // found standard prefix, remove it
        pixelType.erase(0,MF_VIDEO_FORMAT_PREFIX.size());
      }
      std::ostringstream strFriendlyName; 
      strFriendlyName << "Stream index " << streamIndex;
      strFriendlyName << " - Frame size: " << type.width << "x" << type.height;
      strFriendlyName << ", video format: "<<pixelType;
      strFriendlyName << ", frame rate: "<<type.MF_MT_FRAME_RATE;
      videoModes.push_back(strFriendlyName.str());
    }
  }
}

//----------------------------------------------------------------------------
std::string vtkMmfVideoSource::GetRequestedDeviceName()
{
  return GetCaptureDeviceName(this->RequestedVideoFormat.DeviceId);
}

//----------------------------------------------------------------------------
std::string vtkMmfVideoSource::GetActiveDeviceName()
{
  return GetCaptureDeviceName(this->ActiveVideoFormat.DeviceId);
}

//----------------------------------------------------------------------------
PlusStatus vtkMmfVideoSource::AddFrame(unsigned char* bufferData, DWORD bufferSize)
{
  if (!this->Recording)
  {
    LOG_ERROR("vtkMmfVideoSource::AddFrame skipped, not recording anymore");
    return PLUS_SUCCESS;
  }

  int frameSize[3]={0,0,0};
  vtkPlusDataSource* videoSource(NULL);
  if( this->GetFirstVideoSource(videoSource) != PLUS_SUCCESS )
  {
    return PLUS_FAIL;
  }
  videoSource->GetInputFrameSize(frameSize);

  PlusStatus decodingStatus(PLUS_SUCCESS);
  PixelCodec::PixelEncoding encoding(PixelCodec::PixelEncoding_ERROR);
  if (this->ActiveVideoFormat.PixelFormatName.compare("YUY2") == 0)
  {
    if (bufferSize<frameSize[0]*frameSize[1]*2)
    {
      LOG_ERROR("Failed to decode pixel data from YUY2 due to buffer size mismatch");
      return PLUS_FAIL;
    }
    encoding = PixelCodec::PixelEncoding_YUY2;
  }
  else if (this->ActiveVideoFormat.PixelFormatName.compare("MJPG") == 0 )
  {
    encoding = PixelCodec::PixelEncoding_MJPG;
  }
  else if (this->ActiveVideoFormat.PixelFormatName.compare("RGB24") == 0 )
  {
    if (bufferSize<frameSize[0]*frameSize[1]*3)
    {
      LOG_ERROR("Failed to decode pixel data from RGB24 due to buffer size mismatch");
      return PLUS_FAIL;
    }
    encoding = PixelCodec::PixelEncoding_BGR24;
  }
  else
  {
    LOG_ERROR("Unknown pixel type: "<<this->ActiveVideoFormat.PixelFormatName<<" (only YUY2, MJPG and RGB24 are supported)");
    return PLUS_FAIL;
  }

  if( videoSource->GetImageType() == US_IMG_RGB_COLOR )
  {
    decodingStatus = PixelCodec::ConvertToBmp24(PixelCodec::ComponentOrder_RGB, encoding, frameSize[0], frameSize[1], bufferData, (unsigned char*)this->UncompressedVideoFrame.GetScalarPointer());
  }
  else
  {
    decodingStatus = PixelCodec::ConvertToGray(encoding, frameSize[0], frameSize[1], bufferData, (unsigned char*)this->UncompressedVideoFrame.GetScalarPointer());
  }

  if (decodingStatus != PLUS_SUCCESS)
  {
    LOG_ERROR("Error while decoding the grabbed image");
    return PLUS_FAIL;
  }

  this->FrameIndex++;
  vtkPlusDataSource* aSource(NULL);
  if( this->GetFirstVideoSource(aSource) != PLUS_SUCCESS )
  {
    LOG_ERROR("Unable to retrieve the video source in the media foundation capture device.");
    return PLUS_FAIL;
  }
  PlusStatus status = aSource->AddItem(&this->UncompressedVideoFrame, this->FrameIndex); 

  this->Modified();
  return status;
}

//----------------------------------------------------------------------------
void vtkMmfVideoSource::LogListOfCaptureVideoFormats(unsigned int deviceId)
{
  LOG_INFO("Supported vide formats for Device Id "<<deviceId<<" ("<<GetCaptureDeviceName(deviceId)<<")");
  std::vector< std::string > videoModes;
  GetListOfCaptureVideoFormats(videoModes, deviceId);
  for (std::vector< std::string > :: iterator modeIt=videoModes.begin(); modeIt!=videoModes.end(); ++modeIt)
  {
    LOG_INFO("  "<<(*modeIt));
  }
}

//----------------------------------------------------------------------------
void vtkMmfVideoSource::LogListOfCaptureDevices()
{
  LOG_INFO("Found capture devices:");
  std::vector< std::string > deviceNames;
  GetListOfCaptureDevices(deviceNames);
  int id=0;
  for (std::vector< std::string > :: iterator deviceNameIt=deviceNames.begin(); deviceNameIt!=deviceNames.end(); ++deviceNameIt, id++)
  {
    LOG_INFO("  "<<id<<": "<<(*deviceNameIt));
  }
}

//----------------------------------------------------------------------------
std::string vtkMmfVideoSource::GetCaptureDeviceName(unsigned int deviceId)
{
  std::wstring nameW=MfVideoCapture::MediaFoundationVideoCaptureApi::GetInstance().GetCaptureDeviceName(deviceId);
  std::string name(nameW.begin(), nameW.end());
  return name;
}
