/*=Plus=header=begin======================================================
Program: Plus
Copyright (c) Laboratory for Percutaneous Surgery. All rights reserved.
See License.txt for details.
=========================================================Plus=header=end*/

#ifndef __vtkPlusDevice_h
#define __vtkPlusDevice_h

#include "PlusConfigure.h"
#include "vtkDataCollectionExport.h"

#include "PlusCommon.h"
#include "vtkStdString.h"
#include "StreamBufferItem.h"
#include "TrackedFrame.h"

#include "vtkImageAlgorithm.h"
#include "vtkMultiThreader.h"
#include "vtkPlusChannel.h"

#include <string>

class TrackedFrame;
class vtkDataCollector;
class vtkHTMLGenerator;
class vtkPlusBuffer;
class vtkPlusDataSource;
class vtkXMLDataElement;
class vtkPlusDevice;

typedef std::vector<vtkPlusChannel*> ChannelContainer;
typedef ChannelContainer::const_iterator ChannelContainerConstIterator;
typedef ChannelContainer::iterator ChannelContainerIterator;

typedef std::vector<vtkPlusBuffer*> StreamBufferContainer;
typedef StreamBufferContainer::const_iterator StreamBufferContainerConstIterator;
typedef StreamBufferContainer::iterator StreamBufferContainerIterator;

typedef std::map<int, vtkPlusBuffer*> StreamBufferMapContainer;
typedef StreamBufferMapContainer::const_iterator StreamBufferMapContainerConstIterator;
typedef StreamBufferMapContainer::iterator StreamBufferMapContainerIterator;

typedef std::vector<vtkPlusDevice*> DeviceCollection;
typedef std::vector<vtkPlusDevice*>::iterator DeviceCollectionIterator;
typedef std::vector<vtkPlusDevice*>::const_iterator DeviceCollectionConstIterator;

/*!
\class vtkPlusDevice 
\brief Abstract interface for tracker and video devices

vtkPlusDevice is an abstract VTK interface to real-time tracking and imaging
systems.  Derived classes should override the InternalConnect(), InternalDisconnect(), 
GetSdkVersion(), ReadConfiguration(), WriteConfiguration() methods.

\ingroup PlusLibDataCollection
*/
class vtkDataCollectionExport vtkPlusDevice : public vtkImageAlgorithm
{
public:
  class ParamIndexKey
  {
  public:
    static const double NO_DEPTH;
  public:
    ParamIndexKey();

  public:
    double Depth;
    std::string ProbeId;
    PlusImagingMode Mode;
  };
    
public:
  static vtkPlusDevice* New();
  vtkTypeMacro(vtkPlusDevice, vtkImageAlgorithm);
  void PrintSelf(ostream& os, vtkIndent indent);

  static const char* BMODE_PORT_NAME;
  static const char* DEFAULT_TRACKER_REFERENCE_FRAME_NAME;
  static const char* RFMODE_PORT_NAME;
  static const std::string PROBE_SWITCH_ATTRIBUTE_NAME;
  static const std::string DEPTH_SWITCH_ATTRIBUTE_NAME;
  static const std::string MODE_SWITCH_ATTRIBUTE_NAME;
  /*! 
  Probe to see to see if the device is connected to the 
  computer.  This method should be overridden in subclasses. 
  */
  virtual PlusStatus Probe();

  /*! Hardware device SDK version. This method should be overridden in subclasses. */
  virtual std::string GetSdkVersion();

  /*! Read main configuration from xml data */
  virtual PlusStatus ReadConfiguration(vtkXMLDataElement*);

  /*! Write main configuration to xml data */
  virtual PlusStatus WriteConfiguration(vtkXMLDataElement*);

  /*! Connect to device. Connection is needed for recording or single frame acquisition */
  virtual PlusStatus Connect();

  /*!
  Force the hardware to device to acquire a frame
  It requires overriding of the InternalUpdate() function in the child class.
  */
  virtual PlusStatus ForceUpdate();

  /*!
  Disconnect from device.
  This method must be called before application exit, or else the
  application might hang during exit.
  */
  virtual PlusStatus Disconnect();

  /*!
  Return whether or not the device can be reset
  */
  virtual bool IsResettable();

  /*!
  Record incoming data at the specified acquisition rate.  The recording
  continues indefinitely until StopRecording() is called. 
  */
  virtual PlusStatus StartRecording();

  /*! Stop recording */
  virtual PlusStatus StopRecording();

  /*! Return the reference frame */
  static PlusStatus GetToolReferenceFrameFromTrackedFrame(TrackedFrame& aFrame, std::string &aToolReferenceFrameName);

  /*! 
  Get the buffer that is used to hold the data
  There are cases when multiple externally controlled buffers are needed.
  There must always be a valid buffer in the data object, therefore
  the input parameter shall not be NULL.
  */
  //virtual PlusStatus SetBuffer(vtkPlusBuffer* newBuffer);

  /*! 
  Set size of the internal frame buffer, i.e. the number of most recent frames that
  are stored in the video source class internally.
  */
  virtual PlusStatus SetBufferSize(vtkPlusChannel& aChannel, int FrameBufferSize, const char* toolSourceId = NULL);
  /*! Get size of the internal frame buffer. */
  virtual PlusStatus GetBufferSize(vtkPlusChannel& aChannel, int& outVal, const char * toolSourceId = NULL);

  /*! Set recording start time */
  virtual void SetStartTime( double startTime );

  /*! Get recording start time */
  virtual double GetStartTime();

  /*! Is this device a tracker */
  virtual bool IsTracker() const;

  virtual bool IsVirtual() const { return false; }
  /*!
  Reset the device. The actual reset action is defined in subclasses. A reset is typically performed on the users request
  while the device is connected. A reset can be used for zeroing sensors, cancelling an operation in progress, etc.
  */
  virtual PlusStatus Reset();

  /*! Clear all tool buffers */
  void ClearAllBuffers();

  /*! Dump the current state of the device to sequence file (with each tools and buffers) */
  virtual PlusStatus WriteToSequenceFile(const char* filename, bool useCompression = false );

  /*! Make this device into a copy of another device. */
  void DeepCopy(vtkPlusDevice* device);

  /*! Get the internal update rate for this tracking system.  This is the number of buffer entry items sent by the device per second (per tool). */
  double GetInternalUpdateRate() const;

  /*! Get the data source object for the specified Id name, checks both video and tools */
  PlusStatus GetDataSource(const char* aSourceId, vtkPlusDataSource*& aSource);

  /*! Get the tool object for the specified tool name */
  PlusStatus GetTool(const char* aToolSourceId, vtkPlusDataSource*& aTool);
  PlusStatus GetTool(const std::string& aToolSourceId, vtkPlusDataSource*& aTool);

  /*! Get the first active tool among all the source tools */
  PlusStatus GetFirstActiveTool(vtkPlusDataSource*& aTool) const; 

  /*! Get the tool object for the specified tool port name */
  PlusStatus GetToolByPortName( const char* aPortName, vtkPlusDataSource*& aSource); 
  /*! Get the tool object for the specified tool port name */
  PlusStatus GetVideoSourcesByPortName( const char* aPortName, std::vector<vtkPlusDataSource*>& sources); 

  /*! Get the beginning of the tool iterator */
  DataSourceContainerConstIterator GetToolIteratorBegin() const; 

  /*! Get the end of the tool iterator */
  DataSourceContainerConstIterator GetToolIteratorEnd() const;

  /*! Add tool to the device */
  PlusStatus AddTool(vtkPlusDataSource* tool, bool requireUniquePortName=true);

  /*! Get number of images */
  int GetNumberOfTools() const;

  /*! Get the video source for the specified source name */
  PlusStatus GetVideoSource(const char* aSourceId, vtkPlusDataSource*& aVideoSource);

  /*! Get all video sources*/
  std::vector<vtkPlusDataSource*> GetVideoSources() const;

  /*! Get the video source for the specified source index */
  PlusStatus GetVideoSourceByIndex(const int index, vtkPlusDataSource*& aVideoSource);

  /*! Get the first active image object */
  PlusStatus GetFirstVideoSource(vtkPlusDataSource*& anImage); 

  /*! Get the beginning of the image iterator */
  DataSourceContainerConstIterator GetVideoIteratorBegin() const; 

  /*! Get the end of the image iterator */
  DataSourceContainerConstIterator GetVideoIteratorEnd() const;

  /*! Add image to the device */
  PlusStatus AddVideo( vtkPlusDataSource* anImage ); 

  /*! Get number of images */
  int GetNumberOfVideoSources() const;

  /*! Convert tool status to string */
  static std::string ConvertToolStatusToString(ToolStatus status); 

  /*! Convert tool status to TrackedFrameFieldStatus */
  static TrackedFrameFieldStatus ConvertToolStatusToTrackedFrameFieldStatus(ToolStatus status); 

  /*! Convert TrackedFrameFieldStatus to tool status */
  static ToolStatus ConvertTrackedFrameFieldStatusToToolStatus(TrackedFrameFieldStatus fieldStatus); 

  /*! Set Reference name of the tools */
  vtkSetStringMacro(ToolReferenceFrameName);

  /*! Get Reference name of the tools */
  vtkGetStringMacro(ToolReferenceFrameName);

  /*! Is the device correctly configured? */
  vtkGetMacro(CorrectlyConfigured, bool);

  /*! Set the parent data collector */
  virtual void SetDataCollector(vtkDataCollector* _arg);

  /*! Set buffer size of all available tools */
  void SetToolsBufferSize( int aBufferSize ); 

  /*! Set local time offset of all available buffers */
  virtual void SetLocalTimeOffsetSec( double aTimeOffsetSec );
  virtual double GetLocalTimeOffsetSec();

  /*! 
  The subclass will do all the hardware-specific update stuff
  in this function. It should call ToolUpdate() for each tool.
  Note that vtkPlusDevice.cxx starts up a separate thread after
  InternalStartRecording() is called, and that InternalUpdate() is
  called repeatedly from within that thread.  Therefore, any code
  within InternalUpdate() must be thread safe.  You can temporarily
  pause the thread by locking this->UpdateMutex->Lock() e.g. if you
  need to communicate with the device from outside of InternalUpdate().
  A call to this->UpdateMutex->Unlock() will resume the thread.
  */
  virtual PlusStatus InternalUpdate() { return PLUS_SUCCESS; };

  //BTX
  // These are used by static functions in vtkPlusDevice.cxx, and since
  // VTK doesn't generally use 'friend' functions they are public
  // instead of protected.  Do not use them anywhere except inside
  // vtkPlusDevice.cxx.
  vtkRecursiveCriticalSection* UpdateMutex;
  vtkTimeStamp UpdateTime;
  double InternalUpdateRate;  
  //ETX

  /*! Set the acquisition rate */
  virtual double GetAcquisitionRate() const;
  PlusStatus SetAcquisitionRate(double aRate);

  /*! Get whether recording is underway */
  bool IsRecording() const { return (this->Recording != 0); }

  /* Return the id of the device */
  virtual char* GetDeviceId() const { return this->DeviceId; }
  // Set the device Id
  vtkSetStringMacro(DeviceId);

  /*!
  Get the frame number (some devices has frame numbering, otherwise 
  just increment if new frame received)
  */
  vtkGetMacro(FrameNumber, unsigned long);

  /*!
  Get a time stamp in seconds (resolution of milliseconds) for
  the most recent frame.  Time began on Jan 1, 1970.  This timestamp is only
  valid after the Output has been Updated.  Usually set to the
  timestamp for the output if UpdateWithDesiredTimestamp is off,
  otherwise it is the timestamp for the most recent frame, which is not
  necessarily the output
  */
  virtual double GetFrameTimeStamp() { return this->FrameTimeStamp; };

  /*!
  The result of GetOutput() will be the frame closest to DesiredTimestamp
  if it is set and if UpdateWithDesiredTimestamp is set on (default off)
  */
  vtkSetMacro(UpdateWithDesiredTimestamp, int);
  /*!
  The result of GetOutput() will be the frame closest to DesiredTimestamp
  if it is set and if UpdateWithDesiredTimestamp is set on (default off)
  */
  vtkGetMacro(UpdateWithDesiredTimestamp, int);
  /*!
  The result of GetOutput() will be the frame closest to DesiredTimestamp
  if it is set and if UpdateWithDesiredTimestamp is set on (default off)
  */
  vtkBooleanMacro(UpdateWithDesiredTimestamp, int);

  /*!
  Set the desired timestamp. The result of GetOutput() will be the frame closest to DesiredTimestamp
  if it is set and if UpdateWithDesiredTimestamp is set on (default off)
  */
  vtkSetMacro(DesiredTimestamp, double);
  /*! Get the desired timestamp */
  vtkGetMacro(DesiredTimestamp, double);

  /*! Get the timestamp for the video frame returned with desired timestamping */
  vtkGetMacro(TimestampClosestToDesired, double);

  /*! Are we connected? */
  vtkGetMacro(Connected, int);

  /*!
  Set the full-frame size.  This must be an allowed size for the device,
  the device may either refuse a request for an illegal frame size or
  automatically choose a new frame size.
  */
  virtual PlusStatus SetInputFrameSize(vtkPlusDataSource& aSource, int x, int y,  int z);

  /*!
  Set the full-frame size.  This must be an allowed size for the device,
  the device may either refuse a request for an illegal frame size or
  automatically choose a new frame size.
  */
  virtual PlusStatus SetInputFrameSize(vtkPlusDataSource& aSource, int dim[3]) { return this->SetInputFrameSize(aSource, dim[0], dim[1], dim[3]); };

  /*! Get the full-frame size */
  virtual PlusStatus GetInputFrameSize(vtkPlusChannel& aChannel, int &x, int &y, int &z);

  /*! Get the full-frame size */
  virtual PlusStatus GetInputFrameSize(vtkPlusChannel& aChannel, int dim[3]);

  /*! Get the full-frame size */
  virtual PlusStatus GetOutputFrameSize(vtkPlusChannel& aChannel, int &x, int &y, int &z);

  /*! Get the full-frame size */
  virtual PlusStatus GetOutputFrameSize(vtkPlusChannel& aChannel, int dim[3]);

  /*! Set the pixel type (char, unsigned short, ...) */
  virtual PlusStatus SetPixelType(vtkPlusChannel& aChannel, PlusCommon::VTKScalarPixelType pixelType);
  /*! Get the pixel type (char, unsigned short, ...) */
  virtual PlusCommon::VTKScalarPixelType GetPixelType(vtkPlusChannel& aChannel);

  /*! Set the image type (B-mode, RF, ...) provided by the video source. */
  virtual PlusStatus SetImageType(vtkPlusChannel& aChannel, US_IMAGE_TYPE imageType);
  /*! Get the image pixel type (B-mode, RF, ...) */
  virtual US_IMAGE_TYPE GetImageType(vtkPlusChannel& aChannel);

  /*! Add an output channel */
  PlusStatus AddOutputChannel(vtkPlusChannel* aChannel);

  /*! Access the available output channels */
  PlusStatus GetOutputChannelByName(vtkPlusChannel*& aChannel, const char * aChannelId);

  virtual int OutputChannelCount() const { return OutputChannels.size(); }

  ChannelContainerConstIterator GetOutputChannelsStart() const;
  ChannelContainerConstIterator GetOutputChannelsEnd() const;
  ChannelContainerIterator GetOutputChannelsStart();
  ChannelContainerIterator GetOutputChannelsEnd();

  /*! Add an input channel */
  PlusStatus AddInputChannel(vtkPlusChannel* aChannel);

  /*!
  Perform any completion tasks once configured
  */
  virtual PlusStatus NotifyConfigured(){ return PLUS_SUCCESS; }

  /*! 
  Return the latest or desired image frame. This method can be overridden in subclasses 
  Part of the vtkAlgorithm pipeline
  */
  virtual int RequestInformation(vtkInformation *, vtkInformationVector **, vtkInformationVector *);

  /*! 
  Return the latest or desired image frame. This method can be overridden in subclasses 
  Part of the vtkAlgorithm pipeline
  */
  virtual int RequestData(vtkInformation *, vtkInformationVector **, vtkInformationVector *);

  /* Accessors for the grace period value */
  vtkSetMacro(MissingInputGracePeriodSec, double);
  vtkGetMacro(MissingInputGracePeriodSec, double);

  /*!
    Creates a default output channel for the device with the name 'VideoStream'.
    \param addSource If true then for imaging devices a default 'Video' source is added to the output.
  */
  virtual PlusStatus CreateDefaultOutputChannel(bool addSource=true);

  /*! Convenience function for getting the first available video source in the output channels */
  PlusStatus GetFirstActiveOutputVideoSource(vtkPlusDataSource*& aVideoSource);

  /*! Return a list of items that desrcibe what image volumes this device can provide */
  virtual PlusStatus GetImageMetaData(PlusCommon::ImageMetaDataList& imageMetaDataItems);

  /*!
    Return the specified volume from the corresponding device. The assignedImageId and requestedImageId should be the same
    If requestedImageId is empty then GetImage will return the default image and set assignedImageId to the ID of this default image.
    If requestedImageId is not empty then assignedImageId is the same as the requestedImageId.
  */
  virtual PlusStatus GetImage(const std::string& requestedImageId, std::string& assignedImageId,const std::string& imageReferencFrameName, vtkImageData* imageData, vtkMatrix4x4* ijkToReferenceTransform);

  /*!
    Send text message to the device. If a non-NULL pointer is passed as textReceived
    then the device waits for a response and returns it in textReceived.
  */
  virtual PlusStatus SendText(const std::string& textToSend, std::string* textReceived=NULL);

protected:
  static void *vtkDataCaptureThread(vtkMultiThreader::ThreadInfo *data);

  /* Construct a lookup table for indexing channels by depth, mode and probe */
  PlusStatus BuildParameterIndexList(const ChannelContainer& channels, bool& depthSwitchingEnabled, bool& modeSwitchingEnabled, bool& probeSwitchingEnabled, std::vector<ParamIndexKey*>& output );

  /*! Should be overridden to connect to the hardware */
  virtual PlusStatus InternalConnect() { return PLUS_SUCCESS; }

  /*! Release the video driver. Should be overridden to disconnect from the hardware. */ 
  virtual PlusStatus InternalDisconnect() { return PLUS_SUCCESS; };

  /*!
  Called at the end of StartRecording to allow hardware-specific
  actions for starting the recording
  */
  virtual PlusStatus InternalStartRecording() { return PLUS_SUCCESS; };

  /*! 
  Called at the beginning of StopRecording to allow hardware-specific
  actions for stopping the recording
  */
  virtual PlusStatus InternalStopRecording() { return PLUS_SUCCESS; };

  /*!
  This function can be called to add a video item to all video data sources
  */
  PlusStatus AddVideoItemToVideoSources(const std::vector<vtkPlusDataSource*>& videoSources, const PlusVideoFrame& frame, long frameNumber, double unfilteredTimestamp=UNDEFINED_TIMESTAMP, 
    double filteredTimestamp=UNDEFINED_TIMESTAMP, const TrackedFrame::FieldMapType* customFields = NULL);

  /*! 
  This function is called by InternalUpdate() so that the subclasses
  can communicate information back to the vtkTracker base class, which
  will in turn relay the information to the appropriate vtkPlusDataSource.
  */
  PlusStatus ToolTimeStampedUpdate(const char* aToolSourceId, vtkMatrix4x4 *matrix, ToolStatus status, unsigned long frameNumber, double unfilteredtimestamp);

  /*! 
  This function is called by InternalUpdate() so that the subclasses
  can communicate information back to the vtkTracker base class, which
  will in turn relay the information to the appropriate vtkPlusDataSource.
  This function is for devices has no frame numbering, just auto increment tool frame number if new frame received
  */
  PlusStatus ToolTimeStampedUpdateWithoutFiltering(const char* aToolSourceId, vtkMatrix4x4 *matrix, ToolStatus status, double unfilteredtimestamp, double filteredtimestamp);

  /*!
  Helper function used during configuration to locate the correct XML element for a device
  */
  vtkXMLDataElement* FindThisDeviceElement(vtkXMLDataElement* rootXMLElement);
  /*!
  Helper function used during configuration to locate the correct XML element for an output stream
  */
  vtkXMLDataElement* FindOutputChannelElement(vtkXMLDataElement* rootXMLElement, const char* aChannelId);
  /*!
  Helper function used during configuration to locate the correct XML element for an input stream
  */
  vtkXMLDataElement* FindInputChannelElement(vtkXMLDataElement* rootXMLElement, const char* aChannelId);
  /*!
  Method that writes output streams to XML
  */
  virtual void InternalWriteOutputChannels(vtkXMLDataElement* rootXMLElement);
  /*!
  Method that writes output streams to XML
  */
  virtual void InternalWriteInputChannels(vtkXMLDataElement* rootXMLElement);

  vtkSetMacro(CorrectlyConfigured, bool);

  vtkSetMacro(StartThreadForInternalUpdates, bool);
  vtkGetMacro(StartThreadForInternalUpdates, bool); 

  vtkSetMacro(RecordingStartTime, double);
  vtkGetMacro(RecordingStartTime, double); 

  virtual vtkDataCollector* GetDataCollector() { return this->DataCollector; }

  bool HasGracePeriodExpired();

  vtkPlusDevice();
  virtual ~vtkPlusDevice();

protected:
  static const int VIRTUAL_DEVICE_FRAME_RATE;

  /*! Flag to store recording thread state */
  bool ThreadAlive; 

  /* Is device connected */
  int Connected;

  /*! Thread used for acquisition */
  vtkMultiThreader* Threader;

  /*! Recording thread id */
  int ThreadId;

  ChannelContainer  OutputChannels;
  ChannelContainer  InputChannels;

  /*! A stream buffer item to use as a temporary staging point */
  StreamBufferItem* CurrentStreamBufferItem;
  DataSourceContainer Tools; 
  DataSourceContainer VideoSources;

  /*! Reference name of the tools */
  char* ToolReferenceFrameName; 

  /*! Id of the device */
  char* DeviceId;

  vtkDataCollector* DataCollector;

  /*! Acquisition rate */
  double AcquisitionRate;

  /* Flag whether the device is recording */
  int Recording;

  /*! if we want to update according to the frame closest to the timestamp specified by desiredTimestamp */
  double DesiredTimestamp;
  int UpdateWithDesiredTimestamp;
  double TimestampClosestToDesired;

  /*! Requested frame rate in FPS. Actual frame rate may be different. TODO: Is it really the FPS? -Csaba */
  unsigned long FrameNumber; 
  double FrameTimeStamp;

  /*! Set if output needs to be cleared to be cleared before being written */
  int OutputNeedsInitialization;

  /*! Is this device correctly configured? */
  bool CorrectlyConfigured;

  /*!
  If enabled, then a data capture thread is created when the device is connected that regularly calls InternalUpdate.
  This update mechanism is useful for devices that don't provide callback functions but require polling.
  */
  bool StartThreadForInternalUpdates;

  /*! Value to use when mixing data with another temporally calibrated device*/
  double LocalTimeOffsetSec;

  /*! Adjust the device reporting behaviour depending on whether or not a grace period has expired */
  double MissingInputGracePeriodSec;
  /*! Adjust the device reporting behaviour depending on whether or not a grace period has expired */
  double RecordingStartTime;

  /*!
    The list contains the IDs of the tools that have been already reported to be unknown.
    This list is used to only report an unknown tool once (after the connection has been establiushed), not at each
    attempt to access it.
  */
  std::set< std::string > ReportedUnknownTools;

protected:
  /*
  When defining a device, it may be a tracker or imaging device
  These variables allow a device to define which commonly used attributes it is expecting
  */
  bool RequireImageOrientationInConfiguration;
  bool RequirePortNameInDeviceSetConfiguration;

private:
  vtkPlusDevice(const vtkPlusDevice&);  // Not implemented.
  void operator=(const vtkPlusDevice&);  // Not implemented. 
};

#define XML_FIND_DEVICE_ELEMENT_REQUIRED_FOR_READING(deviceConfig, rootConfigElement) \
  if( Superclass::ReadConfiguration(rootConfigElement) != PLUS_SUCCESS )  \
  { \
    LOG_ERROR("Unable to continue reading configuration of "<<this->GetClassName()<<". Generic device configuration reading failed.");  \
    return PLUS_FAIL; \
  } \
  vtkXMLDataElement* deviceConfig = this->FindThisDeviceElement(rootConfigElement);  \
  if (deviceConfig == NULL)  \
  { \
    LOG_ERROR("Unable to continue configuration of "<<this->GetClassName()<<". Could not find corresponding element.");  \
    return PLUS_FAIL; \
  }

#define XML_FIND_DEVICE_ELEMENT_REQUIRED_FOR_WRITING(deviceConfig, rootConfigElement) \
  if( Superclass::WriteConfiguration(rootConfigElement) == PLUS_FAIL ) \
  { \
    LOG_ERROR("Unable to continue writing configuration of "<<this->GetClassName()<<". Generic device configuration writing failed.");  \
    return PLUS_FAIL; \
  } \
  vtkXMLDataElement* deviceConfig = this->FindThisDeviceElement(rootConfigElement); \
  if (deviceConfig == NULL) \
  { \
    LOG_ERROR("Cannot find or add "<<this->GetClassName()<<" device in XML tree"); \
    return PLUS_FAIL; \
  }

#endif
