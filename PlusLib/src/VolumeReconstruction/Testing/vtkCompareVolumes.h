/*=========================================================================

  Program:   Visualization Toolkit
  Module:    vtkSimpleImageFilterExample.h

  Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
  All rights reserved.
  See Copyright.txt or http://www.kitware.com/Copyright.htm for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notice for more information.

=========================================================================*/
// .NAME vtkCompare Volumes - generates a histogram and statistics about volume reconstruction outputs
// .SECTION Description
// Currently this class only supports volume reconstructions of unsigned characters. The executable needs to be provided the following:
//   - A ground truth image
//   - A ground truth alpha image: This is used together with the slices alpha image to identify hole voxels
//   - A reconsructed "test" image
//   - A slices alpha image: The alpha channel if the slices are only pasted into the volume without hole filling

#ifndef __vtkCompareVolumes_h
#define __vtkCompareVolumes_h

#include "PlusConfigure.h"
#include "vtkThreadedImageAlgorithm.h"
#include <vector>

class vtkCompareVolumes : public vtkThreadedImageAlgorithm 
{
public:
  static vtkCompareVolumes *New();
  vtkTypeMacro(vtkCompareVolumes,vtkThreadedImageAlgorithm);

  // Description:
  // Set the Input1 of this filter. 
  virtual void SetInputGT(vtkDataObject *input) { this->SetInputData_vtk5compatible(0,input);};
  
  // Description:
  // Set the Input2 of this filter.
  virtual void SetInputGTAlpha(vtkDataObject *input) { this->SetInputData_vtk5compatible(1,input);};

  // Description:
  // Set the Input3 of this filter. 
  virtual void SetInputTest(vtkDataObject *input) { this->SetInputData_vtk5compatible(2,input);};
  
  // Description:
  // Set the Input4 of this filter.
  virtual void SetInputTestAlpha(vtkDataObject *input) { this->SetInputData_vtk5compatible(3,input);};
  
  // Description:
  // Set the Input5 of this filter.
  virtual void SetInputSliceAlpha(vtkDataObject *input) { this->SetInputData_vtk5compatible(4,input);};

  // Description:
  // Output the images resulting from this filter
  vtkImageData* GetOutputTrueDifferenceImage() {return this->GetOutput(0);}
  vtkImageData* GetOutputAbsoluteDifferenceImage() {return this->GetOutput(1);}

  vtkGetMacro(RMS,double);
  vtkSetMacro(RMS,double);

  vtkGetMacro(TrueMean,double);
  vtkSetMacro(TrueMean,double);
  vtkGetMacro(TrueStdev,double);
  vtkSetMacro(TrueStdev,double);
  vtkGetMacro(TrueMedian,double);
  vtkSetMacro(TrueMedian,double);
  vtkGetMacro(TrueMinimum,double);
  vtkSetMacro(TrueMinimum,double);
  vtkGetMacro(TrueMaximum,double);
  vtkSetMacro(TrueMaximum,double);
  vtkGetMacro(True5thPercentile,double);
  vtkSetMacro(True5thPercentile,double);
  vtkGetMacro(True95thPercentile,double);
  vtkSetMacro(True95thPercentile,double);

  vtkGetMacro(AbsoluteMean,double);
  vtkSetMacro(AbsoluteMean,double);
  vtkGetMacro(AbsoluteStdev,double);
  vtkSetMacro(AbsoluteStdev,double);
  vtkGetMacro(AbsoluteMedian,double);
  vtkSetMacro(AbsoluteMedian,double);
  vtkGetMacro(AbsoluteMinimum,double);
  vtkSetMacro(AbsoluteMinimum,double);
  vtkGetMacro(AbsoluteMaximum,double);
  vtkSetMacro(AbsoluteMaximum,double);
  vtkGetMacro(Absolute5thPercentile,double);
  vtkSetMacro(Absolute5thPercentile,double);
  vtkGetMacro(Absolute95thPercentile,double);
  vtkSetMacro(Absolute95thPercentile,double);
  vtkGetMacro(NumberOfHoles,int);
  vtkSetMacro(NumberOfHoles,int);
  vtkGetMacro(NumberOfFilledHoles,int);
  vtkSetMacro(NumberOfFilledHoles,int);
  vtkGetMacro(NumberVoxelsVisible,int);
  vtkSetMacro(NumberVoxelsVisible,int);

  vtkGetMacro(AbsoluteMeanWithHoles,double);
  vtkSetMacro(AbsoluteMeanWithHoles,double);

  int* GetTrueHistogramPtr() {return TrueHistogram;}
  int* GetAbsoluteHistogramPtr() {return AbsoluteHistogram;}
  int* GetAbsoluteHistogramWithHolesPtr() {return AbsoluteHistogramWithHoles;}
  void incTrueHistogramAtIndex(int value) {int index = value + 256; TrueHistogram[index]++;}
  void incAbsoluteHistogramAtIndex(int value) {int index = value; AbsoluteHistogram[index]++;}
  void incAbsoluteHistogramWithHolesAtIndex(int value) {int index = value; AbsoluteHistogramWithHoles[index]++;}
  void resetTrueHistogram() {for (int i = 0; i < 511; i++) TrueHistogram[i] = 0;}
  void resetAbsoluteHistogram() {for (int i = 0; i < 256; i++) AbsoluteHistogram[i] = 0;}
  void resetAbsoluteHistogramWithHoles() {for (int i = 0; i < 256; i++) AbsoluteHistogramWithHoles[i] = 0;}

protected:
  vtkCompareVolumes();
  ~vtkCompareVolumes() {};

  double RMS;
  double TrueMean,     TrueStdev,     TrueMedian,     TrueMinimum,     TrueMaximum,     True95thPercentile,     True5thPercentile;
  double AbsoluteMean, AbsoluteStdev, AbsoluteMedian, AbsoluteMinimum, AbsoluteMaximum, Absolute95thPercentile, Absolute5thPercentile;
  double AbsoluteMeanWithHoles;
  int TrueHistogram[511];
  int AbsoluteHistogram[256];
  int AbsoluteHistogramWithHoles[256];
  int NumberOfHoles;
  int NumberOfFilledHoles;
  int NumberVoxelsVisible;

  virtual int RequestInformation (vtkInformation *, vtkInformationVector**, vtkInformationVector *);

  void ThreadedRequestData (vtkInformation* request,
                            vtkInformationVector** inputVector,
                            vtkInformationVector* outputVector,
                            vtkImageData ***inData, vtkImageData **outData,
                            int ext[6], int id);

  virtual int FillInputPortInformation(int port, vtkInformation* info);


private:
  vtkCompareVolumes(const vtkCompareVolumes&);  // Not implemented.
  void operator=(const vtkCompareVolumes&);  // Not implemented.

};

#endif







