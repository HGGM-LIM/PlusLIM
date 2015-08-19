/*=Plus=header=begin======================================================
  Program: Plus
  Copyright (c) Laboratory for Percutaneous Surgery. All rights reserved.
  See License.txt for details.
=========================================================Plus=header=end*/ 

/*=========================================================================
Date: Ag 2015
Authors include:
- Mikael Brudfors [*] brudfors@hggm.es
- Javier Pascau [*][ç] jpascau@hggm.es
[*] Laboratorio de Imagen Medica, Hospital Gregorio Maranon - http://image.hggm.es/
[ç] Departamento de Bioingeniería e Ingeniería Aeroespacial. Universidad Carlos III de Madrid
=========================================================================*/

#ifndef __vtkPlusShowProbeDialogCommand_h
#define __vtkPlusShowProbeDialogCommand_h

#include "vtkPlusServerExport.h"

#include "vtkPlusCommand.h"

class vtkOptimetConoProbeMeasurer;

/*!
  \class vtkPlusStartStopRecordingCommand 
  \brief This command starts and stops capturing with a vtkVirtualDiscCapture capture on the server side. 
  \ingroup PlusLibPlusServer
 */ 
class vtkPlusServerExport vtkPlusShowProbeDialogCommand : public vtkPlusCommand
{
public:

  static vtkPlusShowProbeDialogCommand *New();
  vtkTypeMacro(vtkPlusShowProbeDialogCommand, vtkPlusCommand);
  virtual void PrintSelf( ostream& os, vtkIndent indent );
  virtual vtkPlusCommand* Clone() { return New(); }

  /*! Executes the command  */
  virtual PlusStatus Execute();

  /*! Read command parameters from XML */
  virtual PlusStatus ReadConfiguration(vtkXMLDataElement* aConfig);

  /*! Write command parameters to XML */
  virtual PlusStatus WriteConfiguration(vtkXMLDataElement* aConfig);

  /*! Get all the command names that this class can execute */
  virtual void GetCommandNames(std::list<std::string> &cmdNames);

  /*! Gets the description for the specified command name. */
  virtual std::string GetDescription(const char* commandName);

  vtkGetStringMacro(ConoProbeDeviceId);
  vtkSetStringMacro(ConoProbeDeviceId);

  void SetNameToShow();

  /*!
    Helper function to get pointer to the ConoProbe device
    \param conoProbeDeviceId ConoProbe device ID..
  */
  vtkOptimetConoProbeMeasurer* GetConoProbeDevice(const char* conoProbeDeviceID);

protected:

  vtkPlusShowProbeDialogCommand();
  virtual ~vtkPlusShowProbeDialogCommand();
  
private:

  char* ConoProbeDeviceId;

  vtkPlusShowProbeDialogCommand( const vtkPlusShowProbeDialogCommand& );
  void operator=( const vtkPlusShowProbeDialogCommand& );
  
};


#endif

