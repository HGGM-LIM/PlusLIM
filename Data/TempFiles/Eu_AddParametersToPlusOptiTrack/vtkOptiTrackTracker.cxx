/*=Plus=header=begin======================================================
Program: Plus
Copyright (c) Laboratory for Percutaneous Surgery. All rights reserved.
See License.txt for details.
=========================================================Plus=header=end*/

#include "PlusConfigure.h"
#include "vtkOptiTrackTracker.h"

#include "PlusMath.h"

#include "vtkMath.h"
#include "vtkMatrix4x4.h"
#include "vtkObjectFactory.h"
#include "vtkPlusDataSource.h"
#include "vtkTransform.h"
#include "vtkXMLDataElement.h"
#include "vtksys/SystemTools.hxx"

#include <math.h>
#include <sstream>


vtkStandardNewMacro(vtkOptiTrackTracker);

//-----------------------------------------------------------------------
vtkOptiTrackTracker::vtkOptiTrackTracker()
{
	this->RequirePortNameInDeviceSetConfiguration = false;

	//Set default VideoType parameter (Tracking Mode).
	this->VideoType = 4;

	// No callback function provided by the device, so the data capture thread will be used to poll the hardware and add new items to the buffer
	this->StartThreadForInternalUpdates = true;

	// Optitrack tracker
	this->OptiTrackTracker = Optitrack::OptitrackTracker::New();

	//Empty Configuration File vector
	this->OptiTrackToolsConfFiles.clear();
}

//-------------------------------------------------------------------------
vtkOptiTrackTracker::~vtkOptiTrackTracker()
{

	if (this->OptiTrackTracker.IsNotNull())
	{
		this->OptiTrackTracker->Close();
		this->OptiTrackTracker = NULL;
	}
}

//-------------------------------------------------------------------------
void vtkOptiTrackTracker::PrintSelf(ostream& os, vtkIndent indent)
{
	Superclass::PrintSelf(os, indent);
}

//-------------------------------------------------------------------------
PlusStatus vtkOptiTrackTracker::InternalConnect()
{
	LOG_TRACE("vtkOptiTrackTracker::Connect");

	// Connect tracker
	int result = this->OptiTrackTracker->Open();

	// Load Calibration file
	this->OptiTrackTracker->SetCalibrationFile(this->CalibrationFile);
	result = this->OptiTrackTracker->LoadCalibration();
	if(result == ResultType::FAILURE)
	{
		LOG_ERROR("Calibration File could not be uploaded");
		return PLUS_FAIL;
	}

	// Set camera parameters read from the PLUS XML configuration file
	result = this->OptiTrackTracker->SetCameraParams(this->Exposition, this->Threshold, this->Illumination, this->VideoType);
	if(result == ResultType::FAILURE)
	{
		LOG_ERROR("Camera settings could not be set");
		return PLUS_FAIL;
	}

	// Add tools from Configuration Files
	std::string ConfFile;
	Optitrack::OptitrackTool::Pointer newTool;
	int resultConfigure = 0;

	for (int toolN = 0; toolN < this->OptiTrackToolsConfFiles.size(); toolN++)
	{
		ConfFile = this->OptiTrackToolsConfFiles.at(toolN);
		LOG_TRACE("Adding Tool");
		newTool = Optitrack::OptitrackTool::New();
		resultConfigure = newTool->ConfigureToolByXmlFile(ConfFile);
		if (resultConfigure == ResultType::SUCCESS)
		{
			this->OptiTrackTracker->AddTrackerTool(newTool);
		}
		else
		{
			LOG_ERROR("Tool not added");
		}
	}


	// Start tracker
	result = this->OptiTrackTracker->StartTracking();

	return PLUS_SUCCESS;
}

//-------------------------------------------------------------------------
PlusStatus vtkOptiTrackTracker::InternalDisconnect()
{
	LOG_TRACE("vtkOptiTrackTracker::Disconnect");

	int result = this->OptiTrackTracker->StopTracking();

	result = this->OptiTrackTracker->Close();

	this->OptiTrackTracker = NULL;

	return this->StopRecording();
}

//-------------------------------------------------------------------------
PlusStatus vtkOptiTrackTracker::InternalUpdate()
{
	LOG_TRACE("vtkOptiTrackTracker::InternalUpdate");

	if (this->OptiTrackTracker->GetState() !=
		Optitrack::OptitrackTracker::OPTITRACK_TRACKER_STATE::STATE_TRACKER_Tracking)
	{
		LOG_ERROR("called Update() when Optitrack was not tracking");
		return PLUS_FAIL;
	}

	// Tools are updated in a different Thread

	// Default to incrementing frame count by one (in case a frame index cannot be retrieved from the tracker for a specific tool)
	this->LastFrameNumber++;
	int defaultToolFrameNumber = this->LastFrameNumber;
	const double toolTimestamp = vtkAccurateTimer::GetSystemTime(); // unfiltered timestamp
	vtkSmartPointer<vtkMatrix4x4> toolToTrackerTransform = vtkSmartPointer<vtkMatrix4x4>::New();
	vtkSmartPointer<vtkMatrix4x4> paramToTrackerTransform = vtkSmartPointer<vtkMatrix4x4>::New();

	Optitrack::OptitrackTool::Pointer currentTool;
	for (DataSourceContainerConstIterator it = this->GetToolIteratorBegin(); it != this->GetToolIteratorEnd(); ++it)
	{
		ToolStatus toolFlags = TOOL_OK;
		toolToTrackerTransform->Identity();
		vtkPlusDataSource* trackerTool = it->second;
		std::string toolName = trackerTool->GetPortName();

		if (toolName.find("Parameters")){

			if (this->OptiTrackTracker->GetOptitrackToolByName(toolName))
			{
				currentTool = this->OptiTrackTracker->GetOptitrackToolByName(toolName);
				// Get the data from tool

				if (!currentTool->IsTracked())
				{
					toolFlags = TOOL_MISSING;
				}
				else
				{
					ToolStatus toolFlags = TOOL_OK;
					

					// Here we add the parameters of the tools to the next id

					std::string ParamsToolName = trackerTool->GetPortName();
					ParamsToolName = "Parameters" + ParamsToolName;

					//PlusTransformName toolTransformName(ParamsToolName.c_str(), this->GetToolReferenceFrameName());
					//ParamsToolName = toolTransformName.GetTransformName();

					vtkPlusDataSource* trackerToolParam = NULL;
					this->GetToolByPortName(ParamsToolName.c_str(), trackerToolParam);
					paramToTrackerTransform->Identity();
					this->GetTransformMatrix(toolName, toolToTrackerTransform);
					int numberOfSeenMarkers = currentTool->GetNumberOfSeenMarkers();
					paramToTrackerTransform->SetElement(0, 3, numberOfSeenMarkers);

					++this->FrameNumber;
					// Setting the timestamp
					const double unfilteredTimestamp = vtkAccurateTimer::GetSystemTime();
					this->ToolTimeStampedUpdate(trackerTool->GetSourceId(), toolToTrackerTransform, toolFlags, this->FrameNumber, unfilteredTimestamp);
					this->ToolTimeStampedUpdate(trackerToolParam->GetSourceId(), paramToTrackerTransform, toolFlags, this->FrameNumber, unfilteredTimestamp);

				}

			}

		}

		

	}

	return PLUS_SUCCESS;
}

//----------------------------------------------------------------------------
PlusStatus vtkOptiTrackTracker::ReadConfiguration(vtkXMLDataElement* rootConfigElement)
{
	XML_FIND_DEVICE_ELEMENT_REQUIRED_FOR_READING(deviceConfig, rootConfigElement);


	int exposition = 0;
	if (deviceConfig->GetScalarAttribute("Exposition", exposition))
	{
		{
			this->Exposition = static_cast<unsigned short>(exposition);
		}
	}

	int threshold = 0;
	if (deviceConfig->GetScalarAttribute("Threshold", threshold))
	{
		{
			this->Threshold = static_cast<unsigned short>(threshold);
		}
	}

	int Illumination = 0;
	if (deviceConfig->GetScalarAttribute("Illumination", Illumination))
	{
		{
			this->Illumination = static_cast<unsigned short>(Illumination);
		}
	}

	int videoType = 0;
	if (deviceConfig->GetScalarAttribute("VideoType", videoType))
	{
		{
			this->VideoType = static_cast<unsigned short>(videoType);
		}
	}

	std::string CalibrationFile = std::string(deviceConfig->GetAttribute("CalibrationFile"));

	this->CalibrationFile = CalibrationFile;

	XML_FIND_NESTED_ELEMENT_REQUIRED(dataSourcesElement, deviceConfig, "DataSources");

	for (int nestedElementIndex = 0; nestedElementIndex < dataSourcesElement->GetNumberOfNestedElements(); nestedElementIndex++)
	{
		vtkXMLDataElement* toolDataElement = dataSourcesElement->GetNestedElement(nestedElementIndex);
		if (STRCASECMP(toolDataElement->GetName(), "DataSource") != 0)
		{
			// if this is not a data source element, skip it
			continue;
		}
		if (toolDataElement->GetAttribute("Type") != NULL && STRCASECMP(toolDataElement->GetAttribute("Type"), "Tool") != 0)
		{
			// if this is not a Tool element, skip it
			continue;
		}
		const char* toolId = toolDataElement->GetAttribute("Id");
		if (toolId == NULL)
		{
			LOG_ERROR("Failed to initialize Optitrack tool: DataSource Id is missing");
			continue;
		}
		PlusTransformName toolTransformName(toolId, this->GetToolReferenceFrameName());
		std::string toolSourceId = toolTransformName.GetTransformName();
		vtkPlusDataSource* trackerTool = NULL;

		if (this->GetTool(toolSourceId, trackerTool) != PLUS_SUCCESS || trackerTool == NULL)
		{
			LOG_ERROR("Failed to get Optitrack tool: " << toolSourceId);
			continue;
		}

		trackerTool->SetPortName(toolId);

		std::string xmlFile = std::string(toolDataElement->GetAttribute("ConfFile"));

		bool cond = !xmlFile.empty() && xmlFile != "Parameters";

		if (!xmlFile.empty() && xmlFile!="Parameters")
		{
			this->OptiTrackToolsConfFiles.push_back(xmlFile);
			std::cout << xmlFile << std::endl;
			/*
			//If tool is added properly from XML configuration we create a new fake-tool for extra information parameters
			std::string toolIdpre = toolId;
			std::string toolIdParameters = "Parameters" + toolIdpre;

			PlusTransformName toolTransformName(toolIdParameters, this->GetToolReferenceFrameName());
			std::string toolSourceId = toolTransformName.GetTransformName();

			vtkPlusDataSource* trackerToolParams = vtkPlusDataSource::New();
			trackerToolParams->SetPortName(toolSourceId.c_str());
			trackerToolParams->SetSourceId(toolSourceId.c_str());
			trackerToolParams->SetReferenceCoordinateFrameName(this->GetToolReferenceFrameName());

			this->AddTool(trackerToolParams, true);

			if (this->GetTool(toolSourceId, trackerToolParams) != PLUS_SUCCESS || trackerTool == NULL)
			{
			LOG_ERROR("Failed to get Optitrack tool: " << toolIdParameters);
			continue;
			}
			*/



			

		}

	}

	return PLUS_SUCCESS;
}

//----------------------------------------------------------------------------
PlusStatus vtkOptiTrackTracker::WriteConfiguration(vtkXMLDataElement* rootConfigElement)
{
	XML_FIND_DEVICE_ELEMENT_REQUIRED_FOR_WRITING(deviceConfig, rootConfigElement);

	deviceConfig->SetIntAttribute("Exposition", static_cast<unsigned int>(this->Exposition));
	deviceConfig->SetIntAttribute("Threshold", static_cast<unsigned int>(this->Threshold));
	deviceConfig->SetIntAttribute("Illumination", static_cast<unsigned int>(this->Illumination));
	deviceConfig->SetIntAttribute("VideoType", static_cast<unsigned int>(this->VideoType));
	deviceConfig->SetAttribute("CalibrationFile", this->CalibrationFile.c_str());

	return PLUS_SUCCESS;
}

//----------------------------------------------------------------------------
void vtkOptiTrackTracker::GetTransformMatrix(std::string toolName, vtkMatrix4x4* transformMatrix)
{

	Optitrack::OptitrackTool::Pointer tool = this->OptiTrackTracker->GetOptitrackToolByName(toolName);
	transformMatrix->Identity();
	int rotIndex = 0;
	for (int col = 0; col < 4; col++)
	{
		for (int row = 0; row < 3; row++)
		{
			transformMatrix->SetElement(row, col, tool->GetTransformMatrix()(row,col));
		}
	}

}