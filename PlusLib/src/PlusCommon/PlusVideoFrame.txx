/*=Plus=header=begin======================================================
Program: Plus
Copyright (c) Laboratory for Percutaneous Surgery. All rights reserved.
See License.txt for details.
=========================================================Plus=header=end*/

#include "PlusConfigure.h"
#include "vtkImageData.h"

//----------------------------------------------------------------------------
template<typename ScalarType> PlusStatus PlusVideoFrame::DeepCopyVtkVolumeToItkVolume( vtkImageData* inFrame, typename itk::Image< ScalarType, 3 >::Pointer outFrame )
{
  LOG_TRACE("PlusVideoFrame::ConvertVtkImageToItkImage"); 

  if ( inFrame == NULL )
  {
    LOG_ERROR("Failed to convert vtk image to itk image - input image is NULL!"); 
    return PLUS_FAIL; 
  }

  if ( outFrame.IsNull() )
  {
    LOG_ERROR("Failed to convert vtk image to itk image - output image is NULL!"); 
    return PLUS_FAIL; 
  }

  if( PlusVideoFrame::GetNumberOfBytesPerScalar(inFrame->GetScalarType()) != sizeof(ScalarType) )
  {
    LOG_ERROR("Mismatch between input and output scalar types. In: " << PlusVideoFrame::GetStringFromVTKPixelType(inFrame->GetScalarType()));
    return PLUS_FAIL;
  }

  // convert vtkImageData to itkImage 
  vtkSmartPointer<vtkImageExport> imageExport = vtkSmartPointer<vtkImageExport>::New(); 
  imageExport->SetInputData_vtk5compatible(inFrame); 
  imageExport->Update(); 

  int extent[6]={0,0,0,0,0,0}; 
  inFrame->GetExtent(extent); 

  double width = extent[1] - extent[0] + 1; 
  double height = extent[3] - extent[2] + 1;
  double thickness = extent[5] - extent[4] + 1;
  typename itk::Image< ScalarType, 3 >::SizeType size;
  size[0] = width;
  size[1] = height;
  size[2] = thickness;
  typename itk::Image< ScalarType, 3 >::IndexType start;
  start[0]=0;
  start[1]=0;
  start[2]=0;
  typename itk::Image< ScalarType, 3 >::RegionType region;
  region.SetSize(size);
  region.SetIndex(start);
  outFrame->SetRegions(region);
  try 
  {
    outFrame->Allocate();
  }
  catch(itk::ExceptionObject & err)
  {
    LOG_ERROR("Failed to allocate memory for the image conversion: " << err.GetDescription() ); 
    return PLUS_FAIL; 
  }

  imageExport->Export( outFrame->GetBufferPointer() ); 

  return PLUS_SUCCESS;
}

//----------------------------------------------------------------------------
template<typename ScalarType> PlusStatus PlusVideoFrame::DeepCopyVtkVolumeToItkImage( vtkImageData* inFrame, typename itk::Image< ScalarType, 2 >::Pointer outFrame )
{
  LOG_TRACE("PlusVideoFrame::ConvertVtkImageToItkImage"); 

  if ( inFrame == NULL )
  {
    LOG_ERROR("Failed to convert vtk image to itk image - input image is NULL!"); 
    return PLUS_FAIL; 
  }

  if ( outFrame.IsNull() )
  {
    LOG_ERROR("Failed to convert vtk image to itk image - output image is NULL!"); 
    return PLUS_FAIL; 
  }

  if( PlusVideoFrame::GetNumberOfBytesPerScalar(inFrame->GetScalarType()) != sizeof(ScalarType) )
  {
    LOG_ERROR("Mismatch between input and output scalar types. In: " << PlusVideoFrame::GetStringFromVTKPixelType(inFrame->GetScalarType()));
    return PLUS_FAIL;
  }

  int extent[6]={0,0,0,0,0,0}; 
  inFrame->GetExtent(extent);
  
  if( extent[5]-extent[4] > 1 )
  {
	LOG_WARNING("3D volume sent in to PlusVideoFrame::DeepCopyVtkVolumeToItkImage. Only first slice will be copied.");
  }

  // convert vtkImageData to itkImage 
  vtkSmartPointer<vtkImageExport> imageExport = vtkSmartPointer<vtkImageExport>::New(); 
  imageExport->SetInputData_vtk5compatible(inFrame); 
  imageExport->Update(); 

  double width = extent[1] - extent[0] + 1; 
  double height = extent[3] - extent[2] + 1;

  typename itk::Image< ScalarType, 2 >::SizeType size;
  size[0] = width;
  size[1] = height;
  typename itk::Image< ScalarType, 2 >::IndexType start;
  start[0]=0;
  start[1]=0;
  typename itk::Image< ScalarType, 2 >::RegionType region;
  region.SetSize(size);
  region.SetIndex(start);
  outFrame->SetRegions(region);
  try 
  {
    outFrame->Allocate();
  }
  catch(itk::ExceptionObject & err)
  {
    LOG_ERROR("Failed to allocate memory for the image conversion: " << err.GetDescription() ); 
    return PLUS_FAIL; 
  }

  imageExport->Export( outFrame->GetBufferPointer() ); 

  return PLUS_SUCCESS;
}