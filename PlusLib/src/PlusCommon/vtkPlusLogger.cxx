/*=Plus=header=begin======================================================
Program: Plus
Copyright (c) Laboratory for Percutaneous Surgery. All rights reserved.
See License.txt for details.
=========================================================Plus=header=end*/

#ifdef _WIN32
#include <Windows.h> // required for setting the text color on the console output
#else
#include <errno.h> // required for getting last error on linux
#endif

#include "PlusConfigure.h"
#include "vtkCommand.h"
#include "vtkObjectFactory.h"
#include "vtkPlusLogger.h"
#include "vtkRecursiveCriticalSection.h"
#include "vtksys/SystemTools.hxx" 
#include <sstream>
#include <string>

//-----------------------------------------------------------------------------

vtkPlusLogger* vtkPlusLogger::m_pInstance = NULL;

//-----------------------------------------------------------------------------

vtkStandardNewMacro(vtkPlusLoggerOutputWindow);

//-----------------------------------------------------------------------------
namespace
{
  vtkSimpleRecursiveCriticalSection LoggerCreationCriticalSection;
}

//-----------------------------------------------------------------------------

void vtkPlusLoggerOutputWindow::ReplaceNewlineBySeparator(std::string &str)
{
  std::replace( str.begin(), str.end(), '\r', '|' );
  std::replace( str.begin(), str.end(), '\n', '|' );
}

//-------------------------------------------------------
vtkPlusLoggerOutputWindow::vtkPlusLoggerOutputWindow()
{
}

//-------------------------------------------------------
vtkPlusLoggerOutputWindow::~vtkPlusLoggerOutputWindow()
{
}

//-------------------------------------------------------
void vtkPlusLoggerOutputWindow::DisplayText(const char* text)
{
  if(text==NULL)
  {
    return;
  }
  std::string textStr=text;
  ReplaceNewlineBySeparator(textStr);
  LOG_INFO("VTK log: " << text);
}

//-------------------------------------------------------
void vtkPlusLoggerOutputWindow::DisplayErrorText(const char* text)
{
  if(text == NULL)
  {
    return;
  }
  std::string textStr = text;
  ReplaceNewlineBySeparator(textStr);
  LOG_ERROR("VTK log: " << textStr);

#ifdef _WIN32
  DWORD lastErr=GetLastError();
  LOG_ERROR("Last error: " << lastErr);
#else
  LOG_ERROR("Last error: " << strerror(errno));
#endif

  this->InvokeEvent(vtkCommand::ErrorEvent, (void*)text);
}

//-------------------------------------------------------
void vtkPlusLoggerOutputWindow::DisplayWarningText(const char* text)
{
  if(text==NULL)
  {
    return;
  }
  std::string textStr=text;
  ReplaceNewlineBySeparator(textStr);
  LOG_WARNING("VTK log: " << textStr);
  this->InvokeEvent(vtkCommand::WarningEvent,(void*) text);
}

//-------------------------------------------------------
void vtkPlusLoggerOutputWindow::DisplayGenericWarningText(const char* text)
{
  if(text==NULL)
  {
    return;
  }
  std::string textStr=text;
  ReplaceNewlineBySeparator(textStr);
  LOG_WARNING("VTK log: " << textStr);
}

//-------------------------------------------------------
void vtkPlusLoggerOutputWindow::DisplayDebugText(const char* text)
{
  if(text==NULL)
  {
    return;
  }
  std::string textStr=text;
  ReplaceNewlineBySeparator(textStr);
  LOG_DEBUG("VTK log: " << textStr);
} 

//-------------------------------------------------------
void vtkPlusLoggerOutputWindow::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);
  os << indent << "VTK logs are redirected to Plus logger" << endl;
} 

//-------------------------------------------------------
vtkPlusLogger::vtkPlusLogger()
{
  m_CriticalSection = vtkRecursiveCriticalSection::New();

  m_LogLevel = LOG_LEVEL_INFO;

  // redirect VTK error logs to the Plus logger
  vtkSmartPointer<vtkPlusLoggerOutputWindow> vtkLogger = vtkSmartPointer<vtkPlusLoggerOutputWindow>::New();
  vtkOutputWindow::SetInstance(vtkLogger);

  this->m_LogStream << "time|level|timeoffset|message|location" << std::endl; 
}

//-------------------------------------------------------
vtkPlusLogger::~vtkPlusLogger()
{
  // Disconnect VTK error logging from the Plus logger (restore default VTK logging)
  vtkOutputWindow::SetInstance(NULL);

  if ( this->m_CriticalSection != NULL ) 
  {
    this->m_CriticalSection->Delete(); 
    this->m_CriticalSection = NULL; 
  } 

  if ( this->m_FileStream.is_open() )
  {
    this->m_FileStream.close(); 
  }
}

//-------------------------------------------------------
vtkPlusLogger* vtkPlusLogger::Instance() 
{
  if (m_pInstance == NULL)
  {
    PlusLockGuard<vtkSimpleRecursiveCriticalSection> loggerCreationGuard(&LoggerCreationCriticalSection);
    if( m_pInstance != NULL )
    {
      return m_pInstance;
    }

    m_pInstance = new vtkPlusLogger; 
    vtkPlusConfig::GetInstance(); // set the log file name from the XML config
    std::string strPlusLibVersion = std::string("Software version: ") + 
      PlusCommon::GetPlusLibVersionString(); 

#ifdef _DEBUG
    strPlusLibVersion += " (debug build)";
#endif

    m_pInstance->LogMessage(LOG_LEVEL_INFO, strPlusLibVersion.c_str(), "vtkPlusLogger", __LINE__); 
  }

  return m_pInstance;
}

//-------------------------------------------------------
int vtkPlusLogger::GetLogLevel() 
{ 
  return m_LogLevel; 
}

//-------------------------------------------------------
std::string vtkPlusLogger::GetLogLevelString()
{
  switch (m_LogLevel)
  {
  case LOG_LEVEL_ERROR:
    return "ERROR";
  case LOG_LEVEL_WARNING:
    return "WARNING";
  case LOG_LEVEL_INFO:
    return "INFO";
  case LOG_LEVEL_DEBUG:
    return "DEBUG";
  case LOG_LEVEL_TRACE:
    return "TRACE";
  default:
    return "UNDEFINED";
  }
}

//-------------------------------------------------------
void vtkPlusLogger::SetLogLevel(int logLevel) 
{ 
  if (logLevel==LOG_LEVEL_UNDEFINED)
  {
    // keeping the current log level is requested
    return;
  }

  PlusLockGuard<vtkRecursiveCriticalSection> critSectionGuard(this->m_CriticalSection);
  m_LogLevel = logLevel;
}

//-------------------------------------------------------
void vtkPlusLogger::SetLogFileName(const char* logfilename) 
{ 
  PlusLockGuard<vtkRecursiveCriticalSection> critSectionGuard(this->m_CriticalSection);

  if ( this->m_FileStream.is_open() )
  {
    if (m_LogFileName.compare(logfilename)==0)
    {
      // the file change has not changed and the log file is already created, so there is nothing to do
      return;
    }
    this->m_FileStream.close(); 
  }

  if ( logfilename )
  {
    // set file name and open the file for writing
    m_LogFileName=logfilename; 
    this->m_FileStream.open( m_LogFileName.c_str(), ios::out ); 
  }
  else
  {
    m_LogFileName.clear(); 
  }
}

//-------------------------------------------------------
std::string vtkPlusLogger::GetLogFileName() 
{
  return this->m_LogFileName; 
}

//-------------------------------------------------------
void vtkPlusLogger::LogMessage(LogLevelType level, const char *msg, const char* fileName, int lineNumber, const char* optionalPrefix)
{
  if (m_LogLevel < level)
  {
    // no need to log
    return;
  }

  // If log level is not debug then only print messages for INFO logs (skip the INFO prefix, line numbers, etc.)
  bool onlyShowMessage = (level == LOG_LEVEL_INFO && m_LogLevel <= LOG_LEVEL_INFO);

  std::string timestamp = vtkAccurateTimer::GetInstance()->GetDateAndTimeMSecString();

  std::ostringstream log; 
  switch (level)
  {
  case LOG_LEVEL_ERROR:
    log << "|ERROR";
    break;
  case LOG_LEVEL_WARNING:
    log << "|WARNING"; 
    break;
  case LOG_LEVEL_INFO:
    log << "|INFO"; 
    break;
  case LOG_LEVEL_DEBUG:
    log << "|DEBUG"; 
    break;
  case LOG_LEVEL_TRACE:
    log << "|TRACE"; 
    break;
  default:
    log << "|UNKNOWN"; 
    break;
  }

  // Add timestamp to the log message
  double currentTime = vtkAccurateTimer::GetSystemTime(); 
  log << "|" << std::fixed << std::setw(10) << std::right << std::setfill('0') << currentTime << "|";

  // Either pad out the log or add the optional prefix and pad
  if( optionalPrefix != NULL )
  {
    log << optionalPrefix << "> ";
  }
  else
  {
    log << " ";
  }

  // Add the message to the log
  log << msg << "| in " << fileName << "(" << lineNumber << ")"; // add filename and line number

  {
    PlusLockGuard<vtkRecursiveCriticalSection> critSectionGuard(this->m_CriticalSection);

    if (m_LogLevel >= level)
    { 

#ifdef _WIN32

      // Set the text color to highlight error and warning messages (supported only on windows)
      switch (level)
      {
      case LOG_LEVEL_ERROR:  
        {
          HANDLE hStdout = GetStdHandle(STD_ERROR_HANDLE); 
          SetConsoleTextAttribute(hStdout, FOREGROUND_RED|FOREGROUND_INTENSITY);
        }
        break;
      case LOG_LEVEL_WARNING:
        {
          HANDLE hStdout = GetStdHandle(STD_ERROR_HANDLE); 
          SetConsoleTextAttribute(hStdout, FOREGROUND_RED|FOREGROUND_GREEN|FOREGROUND_INTENSITY);
        }
        break;
      default:
        {
          HANDLE hStdout = GetStdHandle(STD_OUTPUT_HANDLE); 
          SetConsoleTextAttribute(hStdout, FOREGROUND_RED|FOREGROUND_GREEN|FOREGROUND_BLUE);
        }
        break;
      }    
#endif

      if (level>LOG_LEVEL_WARNING)
      {
        if (onlyShowMessage)
        {
          std::cout << msg << std::endl;
        }
        else
        {
          std::cout << log.str() << std::endl;
        }
      }
      else
      {
        if (onlyShowMessage)
        {
          std::cerr << msg << std::endl;
        }
        else
        {
          std::cerr << log.str() << std::endl;
        }
      }

#ifdef _WIN32
      // Revert the text color (supported only on windows)
      if (level==LOG_LEVEL_ERROR || level==LOG_LEVEL_WARNING)
      {
        HANDLE hStdout = GetStdHandle(STD_ERROR_HANDLE); 
        SetConsoleTextAttribute(hStdout, FOREGROUND_RED|FOREGROUND_GREEN|FOREGROUND_BLUE);
      }
#endif

      // Call display message callbacks if higher priority than trace
      if (level < LOG_LEVEL_TRACE)
      {
        std::ostringstream callDataStream;
        callDataStream << level << "|" << log.str();

        InvokeEvent(vtkCommand::UserEvent, (void*)(callDataStream.str().c_str()));      
      }

      // Add to log stream (file)
      this->m_LogStream << std::setw(17) << std::left << timestamp << log.str();
      this->m_LogStream << std::endl; 
    }
  }

  this->Flush();
}

//-------------------------------------------------------
void vtkPlusLogger::Flush()
{
  PlusLockGuard<vtkRecursiveCriticalSection> critSectionGuard(this->m_CriticalSection);

  if ( this->m_FileStream.is_open() )
  {
    this->m_FileStream << this->m_LogStream.str(); 
    this->m_FileStream.flush(); 
    this->m_LogStream.str(""); 
  }
}

//-------------------------------------------------------
void vtkPlusLogger::PrintProgressbar( int percent )
{
  if ( vtkPlusLogger::Instance()->GetLogLevel() != vtkPlusLogger::LOG_LEVEL_INFO )
  {
    return; 
  }

  std::string bar;
  for(int i = 0; i < 50; i++)
  {
    if( i < (percent/2))
    {
      bar.replace(i,1,"=");
    }
    else if( i == (percent/2))
    {
      bar.replace(i,1,">");
    }
    else
    {
      bar.replace(i,1," ");
    }
  }

  std::cout << "\r" "[" << bar << "] ";
  std::cout.width( 3 );
  std::cout << percent << "%     " << std::flush;

  if ( percent == 100)
  {
    std::cout << std::endl << std::endl;
  }
}
