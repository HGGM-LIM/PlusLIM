/*=Plus=header=begin======================================================
  Program: Plus
  Copyright (c) Laboratory for Percutaneous Surgery. All rights reserved.
  See License.txt for details.
=========================================================Plus=header=end*/ 

#ifndef __igtlPlusTrackedFrameMessage_h
#define __igtlPlusTrackedFrameMessage_h

#include "vtkPlusOpenIGTLinkExport.h"

#include <string>

#include "igtlObject.h"
#include "igtlutil/igtl_util.h"
#include "igtlutil/igtl_header.h"
#include "igtlMessageBase.h"
#include "igtl_types.h"
#include "igtl_win32header.h"
#include "TrackedFrame.h"

namespace igtl
{

// This command prevents 4-byte alignment in the struct (which enables m_FrameSize[3])
#pragma pack(1)     /* For 1-byte boundary in memory */

/*! 
  \class PlusTrackedFrameMessage 
  \brief IGTL message helper class for tracked frame messages
  \ingroup PlusLibOpenIGTLink
*/
class vtkPlusOpenIGTLinkExport PlusTrackedFrameMessage: public MessageBase
{
public:
  typedef PlusTrackedFrameMessage         Self;
  typedef MessageBase                     Superclass;
  typedef SmartPointer<Self>              Pointer;
  typedef SmartPointer<const Self>        ConstPointer;

  igtlTypeMacro( igtl::PlusTrackedFrameMessage, igtl::MessageBase );
  igtlNewMacro( igtl::PlusTrackedFrameMessage );

public:
  
  /*! Set Plus TrackedFrame */ 
  PlusStatus SetTrackedFrame( const TrackedFrame& trackedFrame); 

  /*! Get Plus TrackedFrame */ 
  TrackedFrame GetTrackedFrame(); 
  
protected:
  
  struct MessageHeader 
  {
    size_t GetMessageHeaderSize()
    {
      size_t headersize = 0; 
      headersize += sizeof(igtl_uint16);  // m_Version
      headersize += sizeof(igtl_uint16);  // m_ScalarType
      headersize += sizeof(igtl_uint16);   // m_NumberOfComponents
      headersize += sizeof(igtl_uint16);  // m_ImageType
      headersize += sizeof(igtl_uint16) * 3;  // m_FrameSize[3]
      headersize += sizeof(igtl_uint32);  // m_ImageDataSizeInBytes
      headersize += sizeof(igtl_uint32);  // m_XmlDataSizeInBytes

      return headersize; 
    }

    void ConvertEndianness()
    {
      if (igtl_is_little_endian()) 
      {
        m_Version = BYTE_SWAP_INT16(m_Version); 
        m_ScalarType = BYTE_SWAP_INT16(m_ScalarType); 
        m_NumberOfComponents = BYTE_SWAP_INT16(m_NumberOfComponents);
        m_ImageType = BYTE_SWAP_INT16(m_ImageType);
        m_FrameSize[0] = BYTE_SWAP_INT16(m_FrameSize[0]); 
        m_FrameSize[1] = BYTE_SWAP_INT16(m_FrameSize[1]);
        m_FrameSize[2] = BYTE_SWAP_INT16(m_FrameSize[2]);
        m_ImageDataSizeInBytes = BYTE_SWAP_INT32(m_ImageDataSizeInBytes);
        m_XmlDataSizeInBytes = BYTE_SWAP_INT32(m_XmlDataSizeInBytes);
      }
    }

    igtl_uint16 m_Version;          /* data format version number(1)   */
    igtl_uint16 m_ScalarType;      /* scalar type                     */
    igtl_uint16 m_NumberOfComponents; /* number of scalar components */
    igtl_uint16 m_ImageType;          /* image type */
    igtl_uint16 m_FrameSize[3];    /* entire image volume size */
    igtl_uint32 m_ImageDataSizeInBytes; 
    igtl_uint32 m_XmlDataSizeInBytes; 
  };

  virtual int  GetBodyPackSize();
  virtual int  PackBody();
  virtual int  UnpackBody();

  PlusTrackedFrameMessage();
  ~PlusTrackedFrameMessage();

  TrackedFrame m_TrackedFrame; 
  std::string m_TrackedFrameXmlData; 

  MessageHeader m_MessageHeader; 
};

#pragma pack()

} // namespace igtl

#endif 
