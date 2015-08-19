/*=Plus=header=begin======================================================
Program: Plus
Copyright (c) Laboratory for Percutaneous Surgery. All rights reserved.
See License.txt for details.
=========================================================Plus=header=end*/ 

#include "PlusConfigure.h"

#include "vtkPlusCommandProcessor.h"
#include "vtkPlusShowProbeDialogCommand.h"
#include "vtkOptimetConoProbeMeasurer.h"
#include "vtkDataCollector.h"

vtkStandardNewMacro( vtkPlusShowProbeDialogCommand );

static const char SHOW_CMD[] = "ShowProbeDialog";

//----------------------------------------------------------------------------
vtkPlusShowProbeDialogCommand::vtkPlusShowProbeDialogCommand()
: ConoProbeDeviceId(NULL)
{
}

//----------------------------------------------------------------------------
vtkPlusShowProbeDialogCommand::~vtkPlusShowProbeDialogCommand()
{
  SetConoProbeDeviceId(NULL);
}

//----------------------------------------------------------------------------
void vtkPlusShowProbeDialogCommand::SetNameToShow() { SetName(SHOW_CMD); }

//----------------------------------------------------------------------------
void vtkPlusShowProbeDialogCommand::GetCommandNames(std::list<std::string> &cmdNames)
{ 
  cmdNames.clear(); 
  cmdNames.push_back(SHOW_CMD);
}

//----------------------------------------------------------------------------
std::string vtkPlusShowProbeDialogCommand::GetDescription(const char* commandName)
{ 
  std::string desc;
  if (commandName==NULL || STRCASECMP(commandName, SHOW_CMD))
  {
    desc+=SHOW_CMD;
    desc+=": Opens the Probe Dialog part of the Optimet Smart32 SDK. ConoProbeDeviceId: ID of the ConoProbe device.";
  }
  return desc;
}

//----------------------------------------------------------------------------
void vtkPlusShowProbeDialogCommand::PrintSelf( ostream& os, vtkIndent indent )
{
  this->Superclass::PrintSelf( os, indent );
}

//----------------------------------------------------------------------------
PlusStatus vtkPlusShowProbeDialogCommand::ReadConfiguration(vtkXMLDataElement* aConfig)
{  
  if (vtkPlusCommand::ReadConfiguration(aConfig) != PLUS_SUCCESS)
  {
    return PLUS_FAIL;
  }

  SetConoProbeDeviceId(aConfig->GetAttribute("ConoProbeDeviceId"));

  return PLUS_SUCCESS;
}

//----------------------------------------------------------------------------
PlusStatus vtkPlusShowProbeDialogCommand::WriteConfiguration(vtkXMLDataElement* aConfig)
{  
  if (vtkPlusCommand::WriteConfiguration(aConfig)!=PLUS_SUCCESS)
  {
    return PLUS_FAIL;
  }  
  
  XML_WRITE_STRING_ATTRIBUTE_REMOVE_IF_NULL(ConoProbeDeviceId, aConfig);

  return PLUS_SUCCESS;
}

//----------------------------------------------------------------------------
vtkOptimetConoProbeMeasurer* vtkPlusShowProbeDialogCommand::GetConoProbeDevice(const char* conoProbeDeviceID)
{
  vtkDataCollector* dataCollector=GetDataCollector();
  if (dataCollector==NULL)
  {
    LOG_ERROR("Data collector is invalid");    
    return NULL;
  }
  vtkOptimetConoProbeMeasurer* conoProbeDevice = NULL;
  if (conoProbeDeviceID!=NULL)
  {
    // ConoProbe device ID is specified
    vtkPlusDevice* device=NULL;
    if (dataCollector->GetDevice(device, conoProbeDeviceID)!=PLUS_SUCCESS)
    {
      LOG_ERROR("No OptimetConoProbe has been found by the name "<<conoProbeDeviceID);
      return NULL;
    }
    // device found
    conoProbeDevice = vtkOptimetConoProbeMeasurer::SafeDownCast(device);
    if (conoProbeDevice==NULL)
    {
      // wrong type
      LOG_ERROR("The specified device "<<conoProbeDeviceID<<" is not VirtualStreamCapture");
      return NULL;
    }
  }
  else
  {
    // No ConoProbe device id is specified, auto-detect the first one and use that
    for( DeviceCollectionConstIterator it = dataCollector->GetDeviceConstIteratorBegin(); it != dataCollector->GetDeviceConstIteratorEnd(); ++it )
    {
      conoProbeDevice = vtkOptimetConoProbeMeasurer::SafeDownCast(*it);
      if (conoProbeDevice!=NULL)
      {      
        // found a recording device
        break;
      }
    }
    if (conoProbeDevice==NULL)
    {
      LOG_ERROR("No OptimetConoProbe has been found");
      return NULL;
    }
  }  
  return conoProbeDevice;
}

//----------------------------------------------------------------------------
PlusStatus vtkPlusShowProbeDialogCommand::Execute()
{
  LOG_INFO("vtkPlusShowProbeDialogCommand::Execute:");

  if (this->Name==NULL)
  {
    this->QueueStringResponse("Command failed, no command name specified",PLUS_FAIL);
    return PLUS_FAIL;
  }

  vtkOptimetConoProbeMeasurer *conoProbeDevice=GetConoProbeDevice(this->ConoProbeDeviceId);
  if (conoProbeDevice==NULL)
  {
    this->QueueStringResponse(std::string("OptimetConoProbe has not been found (")
      + (this->ConoProbeDeviceId ? this->ConoProbeDeviceId : "auto-detect") + "), "+this->Name+" failed", PLUS_FAIL);
    return PLUS_FAIL;
  }    

  std::string responseMessageBase = std::string("OptimetConoProbe (")+conoProbeDevice->GetDeviceId()+") "+this->Name;
  LOG_INFO("vtkPlusShowProbeDialogCommand::Execute: "<<this->Name);
  if (STRCASECMP(this->Name, SHOW_CMD)==0)
  {
    if (conoProbeDevice->ShowProbeDialog())
    {
      this->QueueStringResponse(std::string("Probe Dialog open."), PLUS_SUCCESS);
      return PLUS_SUCCESS;
    }
    this->QueueStringResponse(responseMessageBase, PLUS_FAIL);
    return PLUS_FAIL;
  }

  this->QueueStringResponse(responseMessageBase + " unknown command: " + this->Name,PLUS_FAIL);
  return PLUS_FAIL;
}
