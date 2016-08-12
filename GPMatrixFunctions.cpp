#include "GPMatrixFunctions.h"

bool            GPMatrixFunctions::_want_to_close              = false;


GPMatrixFunctions::GPMatrixFunctions()
{
}

GPMatrixProgressBar::GPMatrixProgressBar(QWidget* parent)
{
    _progress_dialog = new QProgressDialog(parent);
    (*_progress_dialog).setMinimumDuration(0);

    _use_progress_dialog = true;
    _batch_processing = false;
}

GPMatrixProgressBar::~GPMatrixProgressBar()
{
    delete _progress_dialog;
}

void GPMatrixProgressBar::beginBatch(int number_tasks)
{
    //TODO check number of tasks is feasible?
    _batch_number_tasks = number_tasks;
    _batch_current_task = 0;
    _batch_processing = true;
    ((QWidget*)((*_progress_dialog).parent()))->setCursor(Qt::WaitCursor);

    if (_use_progress_dialog)
    {
       (*_progress_dialog).setWindowModality(Qt::WindowModal);
        (*_progress_dialog).setAutoClose(false);
        (*_progress_dialog).setAutoReset(false);
        (*_progress_dialog).setCancelButtonText(("Cancel"));
        (*_progress_dialog).setRange(0, number_tasks*100);
        (*_progress_dialog).setValue(0);
        (*_progress_dialog).setWindowTitle(QString("Processing task %1 of %2").arg(0).arg(number_tasks));
        (*_progress_dialog).setLabelText("");
        (*_progress_dialog).show();
    }
    qApp->processEvents();
}


void GPMatrixProgressBar::beginProcessing(QString process_name, int process_length)
{
    if (!_batch_processing)
    {
        ((QWidget*)((*_progress_dialog).parent()))->setCursor(Qt::WaitCursor);

        if (_use_progress_dialog)
        {
           (*_progress_dialog).setWindowModality(Qt::WindowModal);
            (*_progress_dialog).setAutoClose(false);
            (*_progress_dialog).setAutoReset(false);
            (*_progress_dialog).setCancelButtonText(("Cancel"));
            (*_progress_dialog).setRange(0, process_length);
            (*_progress_dialog).setValue(0);
            (*_progress_dialog).setWindowTitle("Processing Data");
            (*_progress_dialog).setLabelText(process_name);
            (*_progress_dialog).show();
        }
        qApp->processEvents();
    }
    else
    {
        //beginning new task in batch processing
        _batch_current_task++;
        _current_process_end = process_length;
        _current_process_progress = 0;
        (*_progress_dialog).setValue(100*_batch_current_task);
        (*_progress_dialog).setWindowTitle(QString("Processing participant %1 of %2").arg(_batch_current_task).arg(_batch_number_tasks));
        (*_progress_dialog).setLabelText(process_name);
        int height = (*_progress_dialog).height();
        (*_progress_dialog).resize(300,height);
        (*_progress_dialog).show();

        qApp->processEvents();
    }
}

void GPMatrixProgressBar::updateProgressDialog(int progress, QString label)
{
    if (!_batch_processing)
    {
        (*_progress_dialog).setValue(progress);
        (*_progress_dialog).setLabelText(label);
        (*_progress_dialog).show();
        qApp->processEvents();
    }
    else
    {
        _current_process_progress = progress;
        int batch = 100*(_batch_current_task-1);
        int current = 100*_current_process_progress / _current_process_end; //out of 100
        (*_progress_dialog).setValue(batch + current);
        (*_progress_dialog).setLabelText(label);
        (*_progress_dialog).show();
        qApp->processEvents();
    }
}



 void GPMatrixProgressBar::endProcessing()
{
     if(!_batch_processing)
     {
        ((QWidget*)((*_progress_dialog).parent()))->setCursor(Qt::ArrowCursor);
         (*_progress_dialog).setValue((*_progress_dialog).maximum());
     }
}

 void GPMatrixProgressBar::endBatch()
 {
     ((QWidget*)((*_progress_dialog).parent()))->setCursor(Qt::ArrowCursor);
     (*_progress_dialog).setValue((*_progress_dialog).maximum());
     _batch_processing = false;
 }

 bool GPMatrixProgressBar::wasCanceled()
 {
     return (*_progress_dialog).wasCanceled();
 }

 void GPMatrixFunctions::estimateFixations(mat &RoughM, mat &SmoothM, mat &AutoFixAllM, GrafixSettingsLoader &settingsLoader, GPMatrixProgressBar &gpProgressBar)
 {
      gpProgressBar.beginProcessing("Estimating Fixations",100);
      estimateFixations(RoughM,SmoothM,AutoFixAllM,settingsLoader);
      gpProgressBar.endProcessing();
 }

 void GPMatrixFunctions::estimateFixations(mat &RoughM, mat &SmoothM, mat &AutoFixAllM, GrafixSettingsLoader &settingsLoader)
 {
    if (SmoothM.is_empty())
    {
         return;// If the data is not smoothed we don't allow to estimate fixations.
    }

    //TODO check if velocity is calculated.
    GPMatrixFunctions::fncCalculateVelocity(&SmoothM, settingsLoader);


    // Calculate Fixations mat *p_fixAllM, mat *p_roughM, mat *p_smoothM
    GPMatrixFunctions::fncCalculateFixations(&AutoFixAllM, &RoughM, &SmoothM, settingsLoader);

    //we cannot work with less than one fixation
    if(AutoFixAllM.n_rows < 1) return;

    //GPMatrixFunctions::saveFile(AutoFixAllM, _participant->GetPath(GrafixParticipant::AUTOFIXALL));
    // **** POST-HOC VALIDATION **** (The order is important!)

    // Add columns for the flags in the smooth data if needed
    if (SmoothM.n_cols == 4){
     mat aux = zeros(SmoothM.n_rows, 10);
     aux.cols(0,3) = SmoothM.cols(0,3);
     SmoothM = aux;
    }else{
     SmoothM.cols(7,9).fill(0); // Restart
    }

    //TODO Check if fixations found or next part crasheds


    bool cb_displacement = settingsLoader.LoadSetting(Consts::SETTING_POSTHOC_MERGE_CONSECUTIVE_FLAG).toBool();
    bool cb_velocityVariance = settingsLoader.LoadSetting(Consts::SETTING_POSTHOC_LIMIT_RMS_FLAG).toBool();
    bool cb_minFixation = settingsLoader.LoadSetting(Consts::SETTING_POSTHOC_MIN_DURATION_FLAG).toBool();
    double sliderVelocityVariance = settingsLoader.LoadSetting(Consts::SETTING_POSTHOC_LIMIT_RMS_VAL).toDouble();
    double sliderMinFixation = settingsLoader.LoadSetting(Consts::SETTING_POSTHOC_MIN_DURATION_VAL).toDouble();

    // Merge all fixations with a displacement lower than the displacement threshold
    if (cb_displacement)
        GPMatrixFunctions::fncMergeDisplacementThreshold(&RoughM, &SmoothM, &AutoFixAllM,  settingsLoader);


    // Remove all fixations below the minimun variance
    if (cb_velocityVariance)
        GPMatrixFunctions::fncRemoveHighVarianceFixations( &SmoothM,  &AutoFixAllM, sliderVelocityVariance);


    // Remove all the fixations below the minimun fixation
    if (cb_minFixation)
        GPMatrixFunctions::fncRemoveMinFixations(&AutoFixAllM, &SmoothM, sliderMinFixation);


 }

 void GPMatrixFunctions::smoothRoughMatrix(const mat &RoughM, mat &SmoothM, QString settingsPath, GPMatrixProgressBar &gpProgressBar)
{
     QSettings settings(settingsPath, QSettings::IniFormat);

     if(!settings.contains(Consts::SETTING_SMOOTHING_USE_OTHER_EYE))
     {
         //TODO: Handle errors with user
         return;
     }
     bool copy_eyes = settings.value(Consts::SETTING_SMOOTHING_USE_OTHER_EYE).toBool();
     int expWidth = settings.value(Consts::SETTING_EXP_WIDTH).toInt();
     int expHeight = settings.value(Consts::SETTING_EXP_HEIGHT).toInt();

    // We cut the big file and smooth it by segments, as otherwise it is too big to process and the computer goes crazy.
    int numSegments = (int)(RoughM.n_rows / Consts::SMOOTH_MAXITS) + 1;

    gpProgressBar.beginProcessing("Smoothing Data...",numSegments);

    mat cutM;
    mat smoothedM;

    SmoothM.zeros(RoughM.n_rows, 4);

    uword startIndex, stopIndex;

    for (int j= 1; j <= numSegments; ++j)
    {
        gpProgressBar.updateProgressDialog(j, QString("Smoothing segment number %1 of %2...").arg(j).arg(numSegments));

        if (_want_to_close || gpProgressBar.wasCanceled())
        {
            SmoothM.clear();
            gpProgressBar.endProcessing();
            return;
        }

        startIndex = (j-1) * Consts::SMOOTH_MAXITS ;
        stopIndex =  (j) * Consts::SMOOTH_MAXITS ;
        if (stopIndex > RoughM.n_rows)
            stopIndex = RoughM.n_rows-1;
        cutM =  RoughM.submat(startIndex, 0, stopIndex -1 , RoughM.n_cols -1 );

        //smoothSegment(cutM,smoothedM, copy_eyes);
        SmoothM.rows(startIndex,stopIndex) = smoothSegment(cutM, copy_eyes, expWidth, expHeight);
        SmoothM.rows(startIndex,stopIndex).col(0) = RoughM.submat(startIndex, 0, stopIndex  , 0 );
    }

    // Now, create the new rows for the flags:
    mat newCol = zeros(SmoothM.n_rows,6); // velocity, isSaccadeFlag, INterpolationFlag ,Displacement,  velocity variance,    min fixations
    SmoothM = join_rows(SmoothM,newCol);

    /*for (uword rowi = 0; rowi < SmoothM.n_rows; rowi++ )
    {
        double x = SmoothM.at(rowi,2);
        double y = SmoothM.at(rowi,3);
        if (x == 0)
            SmoothM.at(rowi,2) = -1;
        else if (x == datum::nan)
            SmoothM.at(rowi,2) = -1;
        else
            SmoothM.at(rowi,2) = x / expWidth;

        if (y == 0)
            SmoothM.at(rowi,3) = -1;
        else if (x == datum::nan)
            SmoothM.at(rowi,3) = -1;
        else
            SmoothM.at(rowi,3) = y / expHeight;
    }*/
}

 void GPMatrixFunctions::smoothRoughMatrixTrilateral(const mat &RoughM, GrafixSettingsLoader &settingsLoader, mat *SmoothM, GPMatrixProgressBar *gpProgressBar)
 {
     gpProgressBar->beginProcessing("Smoothing Data with Trilateral Filter...",50);
     smoothRoughMatrixTrilateral(RoughM, settingsLoader, SmoothM);
     gpProgressBar->endProcessing();
 }
void GPMatrixFunctions::smoothRoughMatrixTrilateral(const mat &RoughM, GrafixSettingsLoader &settingsLoader, mat *SmoothM)
{
    //load settings
    bool copy_eyes = settingsLoader.LoadSetting(Consts::SETTING_SMOOTHING_USE_OTHER_EYE).toBool();
    int expWidth = settingsLoader.LoadSetting(Consts::SETTING_EXP_WIDTH).toInt();
    int expHeight = settingsLoader.LoadSetting(Consts::SETTING_EXP_HEIGHT).toInt();
    //first put data into Raw2D object
    Raw2D image_X;
    Raw2D image_Y;
    image_X.sizer(RoughM.n_rows,1);
    image_Y.sizer(RoughM.n_rows,1);

    Raw2D image_Xs;
    Raw2D image_Ys;
    image_Xs.sizer(RoughM.n_rows,1);
    image_Ys.sizer(RoughM.n_rows,1);

    //copy eyes if necessary
    for (uword i = 0; i < RoughM.n_rows; ++i)
    {
      if (RoughM(i,2) == -1 && RoughM(i,4) == -1){
          image_X.put(i, 0, 0);
          image_Y.put(i, 0, 0);
      }else if (copy_eyes && RoughM(i,2)!= -1 && RoughM(i,4) == -1 ) { // Left eyes were detected but not right
          image_X.put(i, 0, RoughM(i,2) * expWidth  );
          image_Y.put(i, 0, RoughM(i,3) * expHeight );
      }else if(copy_eyes && RoughM(i,2) == -1 && RoughM(i,4) != -1){  // Right eyes were detected, not left
          image_X.put(i, 0, RoughM(i,4) * expWidth );
          image_Y.put(i, 0, RoughM(i,5) * expHeight );
      }else if(!copy_eyes && ((RoughM(i,2)!= -1 && RoughM(i,4) == -1) || (RoughM(i,4)!= -1 && RoughM(i,2) == -1) )){
          image_X.put(i, 0, 0);
          image_Y.put(i, 0, 0);
      }else{   // Both eyes were detected
          image_X.put(i, 0, (RoughM(i,2) + RoughM(i,4)) * expWidth /2);  // x
          image_Y.put(i, 0, (RoughM(i,3) + RoughM(i,5)) * expHeight /2);  // y
      }
    }

    double x_param = settingsLoader.LoadSetting(Consts::SETTING_SMOOTHING_SIGMA_R).toDouble() ;
    double y_param = settingsLoader.LoadSetting(Consts::SETTING_SMOOTHING_SIGMA_R).toDouble() ;

    double sigmas = settingsLoader.LoadSetting(Consts::SETTING_SMOOTHING_SIGMA_S).toDouble();
    Raw2D::TrilateralFilter(&image_X,&image_Xs, sigmas,x_param);
    Raw2D::TrilateralFilter(&image_Y, &image_Ys, sigmas,y_param);

    SmoothM->zeros(RoughM.n_rows,10);
    for (uword i = 0; i < RoughM.n_rows; ++i)
    {
        SmoothM->at(i,0) = RoughM.at(i,0);
        SmoothM->at(i,2) = image_Xs.get(i,0);
        SmoothM->at(i,3) = image_Ys.get(i,0);
    }

}

 void GPMatrixFunctions::smoothRoughMatrixFBF(const mat &RoughM, const QString path, const GrafixConfiguration &configuration, mat *SmoothM, GPMatrixProgressBar *gpProgressBar)
 {
     gpProgressBar->beginProcessing("Smoothing Data...",50);
     smoothRoughMatrixFBF(RoughM, path, configuration, SmoothM);
     gpProgressBar->endProcessing();
 }

 void GPMatrixFunctions::smoothRoughMatrixFBF(const mat &RoughM, const QString path, const GrafixConfiguration &configuration, mat *SmoothM)
 {
    if(RoughM.is_empty()) { return; }

    GrafixSettingsLoader settings(path, configuration);

    bool copy_eyes = settings.LoadSetting(Consts::SETTING_SMOOTHING_USE_OTHER_EYE).toBool();
    int expWidth = settings.LoadSetting(Consts::SETTING_EXP_WIDTH).toInt();
    int expHeight = settings.LoadSetting(Consts::SETTING_EXP_HEIGHT).toInt();
    double sigma_r = settings.LoadSetting(Consts::SETTING_SMOOTHING_SIGMA_R).toDouble();
    double sigma_s = settings.LoadSetting(Consts::SETTING_SMOOTHING_SIGMA_S).toDouble();
    double Xsigma_r = sigma_r / expWidth;
    double Ysigma_r = sigma_r / expHeight;

    // prepare the smooth matrix
    SmoothM->zeros(RoughM.n_rows, 10);

    // create copy of matrix to copy eyes etc
    mat RoughMCopy = RoughM;

    typedef Array_2D<double> image_type;

    uword validSegmentStartIndex = 0;
    uword validSegmentEndIndex = 0;
    bool inValidSegment = false;

    //mark missing data, and get segments inbetween to smotth
    for (uword i = 0; i < RoughM.n_rows; ++i)
    {
        bool leftXMissing = (RoughM(i,2) < 0 || RoughM(i,2) > 1);
        bool leftYMissing = (RoughM(i,3) < 0 || RoughM(i,3) > 1);
        bool rightXMissing = (RoughM(i,4) < 0 || RoughM(i,4) > 1);
        bool rightYMissing = (RoughM(i,5) < 0 || RoughM(i,5) > 1);

        bool leftMissing = leftXMissing || leftYMissing;
        bool rightMissing = rightXMissing || rightYMissing;

        bool missing = copy_eyes ? (leftMissing && rightMissing) : (leftMissing || rightMissing);



        //copy eyes if necessary
        if (copy_eyes && !missing) {
            if (leftMissing && !rightMissing) {
                RoughMCopy(i,2) = RoughM(i,4);
                RoughMCopy(i,3) = RoughM(i,5);
            } else if (rightMissing && !leftMissing) {
                RoughMCopy(i,4) = RoughM(i,2);
                RoughMCopy(i,5) = RoughM(i,3);
            }
        }

        bool lastRow = i == RoughM.n_rows - 1;

        if (inValidSegment) {

            //if at the last row and it is valid, then mark it so!
            if (lastRow && !missing) {
                validSegmentEndIndex  = i;
            }

            if (missing || lastRow) {

                //found a segment - so lets copy it and smooth it
                mat validSegment = RoughMCopy.rows(validSegmentStartIndex, validSegmentEndIndex);

                //no need to smooth just one data point, so copy it
                if (validSegment.n_rows == 1) {
                    SmoothM->at(validSegmentEndIndex,2) = expWidth * ((RoughMCopy(validSegmentEndIndex,2) + RoughMCopy(validSegmentEndIndex,4)) / 2);
                    SmoothM->at(validSegmentEndIndex,3) = expHeight * ((RoughMCopy(validSegmentEndIndex,3) + RoughMCopy(validSegmentEndIndex,5)) / 2);
                } else {
                    // do fbf smoothing on segment
                    image_type image_X(validSegment.n_rows,1);
                    image_type image_Y(validSegment.n_rows,1);

                    image_type filtered_X(validSegment.n_rows,1);
                    image_type filtered_Y(validSegment.n_rows,1);

                    //copy to image_type - there should be no missing data.
                    for (uword j = 0; j < validSegment.n_rows; ++j) {
                        uword dataIndex = j + validSegmentStartIndex;
                        image_X(j,0) = (RoughMCopy(dataIndex,2) + RoughMCopy(dataIndex,4)) / 2;
                        image_Y(j,0) = (RoughMCopy(dataIndex,3) + RoughMCopy(dataIndex,5)) / 2;
                    }

                    //filter the X and Y data
                    GPMatrixFunctions::fast_LBF(image_X, sigma_s, Xsigma_r, false, &filtered_X);
                    GPMatrixFunctions::fast_LBF(image_Y, sigma_s, Ysigma_r, false, &filtered_Y);

                    //copy to the smoothed matrix
                    for (uword j = 0; j < validSegment.n_rows; ++j) {
                        uword dataIndex = j + validSegmentStartIndex;
                        SmoothM->at(dataIndex,2) = filtered_X.at(j,0) * expWidth;
                        SmoothM->at(dataIndex,3) = filtered_Y.at(j,0) * expHeight;
                    }
                }

                inValidSegment = false;
            } else {
                validSegmentEndIndex = i;

            }
        } else {
            if (!missing) {
                validSegmentStartIndex = i;
                validSegmentEndIndex = i;
                inValidSegment = true;
            }
        }

        // copy timestamp
        SmoothM->at(i,0) = RoughM.at(i,0);

        // set missing data code
        if (missing) {
            SmoothM->at(i,2) = -1;
            SmoothM->at(i,3) = -1;
        }
    }
 }



 void GPMatrixFunctions::fast_LBF(Array_2D<double>& image_X, double sigma_s, double Xsigma_r, bool b, Array_2D<double>* filtered_X)
 {
     typedef Array_2D<double> image_type;

     try {
        Image_filter::fast_LBF(image_X,image_X,sigma_s,Xsigma_r,b,filtered_X,filtered_X);
     }
     catch(const std::runtime_error& re) {
         // speciffic handling for runtime_error
         DialogGrafixError::LogNewError(0,"Runtime error: " + QString(re.what()));;
     }
     catch(const std::exception& ex) {
         // speciffic handling for all exceptions extending std::exception, except
         // std::runtime_error which is handled explicitly
         DialogGrafixError::LogNewError(0,"Error occurred: " + QString(ex.what()) +
                                        " with array size: " + QString::number(image_X.x_size()) +
                                        " splitting file and retrying...");
        //split file, process and patch with seam (that is double sigma s length)
        int mid_point = image_X.x_size() / 2;

        int x1_size = mid_point;
        int x2_size = image_X.x_size() - mid_point;
        int seam_size = sigma_s * 2;

        image_type image_X_1(x1_size,1);
        image_type image_X_2(x2_size,1);
        image_type image_X_seam(seam_size,1);

        image_type filt_X_1(x1_size,1);
        image_type filt_X_2(x2_size,1);
        image_type filt_X_seam(seam_size,1);

        for (int i=0; i<x1_size; ++i)
        {
            image_X_1(i,0) = image_X(i,0);
        }
        for (int i=0; i<x2_size; ++i)
        {
            image_X_2(i,0) = image_X(mid_point + i, 0);
        }
        for (int i=0; i<seam_size; ++i)
        {
            image_X_seam(i,0) = image_X(mid_point - (seam_size / 2), 0);
        }

        fast_LBF(image_X_1,sigma_s,Xsigma_r,false,&filt_X_1);
        fast_LBF(image_X_2,sigma_s,Xsigma_r,false,&filt_X_2);
        fast_LBF(image_X_seam,sigma_s,Xsigma_r,false,&filt_X_seam);

        //now to put back together again
        for (int i=0; i<x1_size; ++i)
        {
            (*filtered_X)(i,0) = filt_X_1(i,0);
        }
        for (int i=0; i<x2_size; ++i)
        {
            (*filtered_X)(mid_point+i, 0) = filt_X_2(i,0);
        }
        for (int i=0; i<seam_size; ++i)
        {
            (*filtered_X)(mid_point - (seam_size / 2), 0) = filt_X_seam(i,0);
        }


     }
     catch(...)
     {
         DialogGrafixError::LogNewError(0,"Unknown Error: Parameters - too small?");
     }
 }

 void GPMatrixFunctions::interpolateData(mat &SmoothM, GrafixSettingsLoader settingsLoader, GPMatrixProgressBar &gpProgressBar)
 {
     // Here we interpolate the smoothed data and create an extra column
     // Smooth = [time,0,x,y,velocity,saccadeFlag(0,1), interpolationFlag]

     qDebug() << "Interpolating...";

     if (SmoothM.is_empty())
         return;

     gpProgressBar.beginProcessing("Interpolating Data", 100);


     double hz                   = settingsLoader.LoadSetting(Consts::SETTING_HZ).toDouble();
     double interpolationLatency = settingsLoader.LoadSetting(Consts::SETTING_INTERP_LATENCY).toDouble();
     double displacInterpolation = settingsLoader.LoadSetting(Consts::SETTING_INTERP_MAXIMUM_DISPLACEMENT).toDouble();
     int    gapLenght            = interpolationLatency * hz / 1000;
     double degreesPerPixel   = settingsLoader.LoadSetting(Consts::SETTING_DEGREE_PER_PIX).toDouble();

     mat aux2, aux3;
     uvec fixIndex;

     // Remove previous interpolation data and create the new columns
     mat aux = zeros(SmoothM.n_rows, 11);
     aux.cols(0,3) = SmoothM.cols(0,3);
     SmoothM = aux;

     // Calculate provisional velocities and saccades (cols 4 & 5)
     GPMatrixFunctions::fncCalculateVelocity(&SmoothM, settingsLoader);

     // Iterate through the samples, detecting missing points.
     for(uword i = 0; i < SmoothM.n_rows; ++i) {


         if ( SmoothM(i,2) >= 0 || SmoothM(i,3) >= 0) {
             continue; // Sample is not missing
         }

         qDebug() << endl << "sample missing at: " << i;

         // Go to the previous saccade and calculate the average X and Y.
         int indexPrevSacc = -1;
         for (int j = i; j > 0 ; --j) {
             if (SmoothM(j,5) == 1) {
                 indexPrevSacc = j;
                 break;
             }
         }

         qDebug() << " previous saccade at: " << indexPrevSacc;

         bool previousSaccadeDetected = indexPrevSacc != -1 && indexPrevSacc < (int)i-1;

         if (!previousSaccadeDetected) {
             continue;
         }

         // cut the data for the previous fixation to calculate euclidean distance
         aux2 =  SmoothM.submat(indexPrevSacc, 2, i-1, 3);
         fixIndex =  arma::find(aux2.col(0) > 0); // Remove the -1 values for doing this calculation!!
         aux3 = aux2.rows(fixIndex);

         // get the location of this preceeding fixation
         double xAvg = mean(aux3.col(0));
         double yAvg = mean(aux3.col(1));

         qDebug() << " xAvg: " << xAvg << " yAvg: " << yAvg;

         //euclideanDisPrev = GPMatrixFunctions::fncCalculateEuclideanDistanceSmooth(&aux3);

         // Find the point where the data is back:
         int indexDataBack = -1;
         for (uword j = i; j < SmoothM.n_rows; ++j){
             if (SmoothM(j,2) > 0 && SmoothM(j,3) > 0){
                 indexDataBack = j ;
                 break;
             }
         }

         qDebug() << " indexDataBack: " << indexDataBack;

         if (indexDataBack == -1){
             // if the data doesn't back at some point
             continue;
         }

         // find the next saccade. Start counting from  indexDataBack+2 , as the first point after missing data can be identified as saccade by mistake.
         int indexNextSacc = -1;
         for (uword j = indexDataBack+2 ; j < SmoothM.n_rows; ++j){
             if (SmoothM(j,5) == 1 ){ // Saccade!
                 indexNextSacc = j;
                 break;
             }
         }

         qDebug() << " indexNextSacc: " << indexDataBack;

         // if there is a next saccade:
         bool followingSaccadeDetected = indexNextSacc != -1 && indexDataBack + 2 < indexNextSacc;

         if (!followingSaccadeDetected) {
             continue;
         }

         aux2 =  SmoothM.submat(indexDataBack+2, 2, indexNextSacc , 3 ); // cut the data for the next fixation to calculate euclidean distance
         fixIndex =  arma::find(aux2.col(0) > 0); // Remove the -1 values for doing this calculation!!
         //todo: here again check it found any
         aux3 = aux2.rows(fixIndex);

         // get the location of the following fixation
         double xAvg2 = mean(aux3.col(0));
         double yAvg2 = mean(aux3.col(1));

         qDebug() << " xAvg2: " << xAvg2 << " yAvg2: " << yAvg2;

         // If the eucluclidean distance is similar (less than "displacInterpolation") at both ends and the gap is smaller than the latency, interpolate!
         double xDiff = xAvg - xAvg2;
         double yDiff = yAvg - yAvg2;


         double distance = sqrt((xDiff * xDiff) + (yDiff * yDiff));
         double distanceInDegrees = distance * degreesPerPixel;

         qDebug() << " distance: " << distance << " degrees: " << distanceInDegrees;

         bool interpolationInRange = distanceInDegrees <= displacInterpolation;
         bool interpolationDistanceInRange = indexDataBack - (int)i < gapLenght;
         qDebug() << " inRange: " << interpolationInRange << " inDistance: " << interpolationDistanceInRange;
         if (interpolationInRange && interpolationDistanceInRange) {
             // INTERPOLATE!!!
            qDebug() << "**** Interpolated from  :" << i  << " too:" << indexDataBack << endl;

             SmoothM.rows(i,indexDataBack).col(2).fill(xAvg);
             SmoothM.rows(i,indexDataBack).col(3).fill(yAvg);
             SmoothM.rows(i,indexDataBack).col(6).fill(1);  //Interpolation index!

             GPMatrixFunctions::fncCalculateVelocity(&SmoothM,settingsLoader); // Update.

         }

         i = indexDataBack;

     }

     gpProgressBar.endProcessing();

 }

 // Smoothing algorithm
 // Bilateral filtering algorithm based on Ed Vul (Frank, Vul, & Johnson,2009; based on Durand & Dorsey, 2002).
 // THis algorithm eliminates Jitter in fixations while preserving saccades.
 void GPMatrixFunctions::smoothSegment(mat &cutM, mat &smoothedM, bool copy_eyes){
    const int n_rows = cutM.n_rows+1;
    smoothedM = zeros(n_rows, 4); // [miliseconds, 0, x, y]

    mat allX =  zeros(n_rows,1);
    mat allY =  zeros(n_rows,1);

    // When there are no values for one eye, copy the values from the other eye.
    // Calculate X and Y initial averages. Exclude when the eyes were not detected
    for (uword i = 0; i < cutM.n_rows; ++i){  // Eyes were not detected
        if (cutM.at(i,2) == -1 && cutM.at(i,4) == -1){
            allX.at(i,0) = 0;
            allY.at(i,0) = 0;
        }else if (copy_eyes && cutM.at(i,2)!= -1 && cutM.at(i,4) == -1 ) { // Left eyes were detected but not right
            allX.at(i,0) = cutM.at(i,2);
            allY.at(i,0) = cutM.at(i,3);
        }else if(copy_eyes && cutM.at(i,2) == -1 && cutM.at(i,4) != -1){  // Right eyes were detected, not left
            allX.at(i,0) = cutM.at(i,4);
            allY.at(i,0) = cutM.at(i,5);
        }else if(!copy_eyes && ((cutM.at(i,2)!= -1 && cutM.at(i,4) == -1) || (cutM.at(i,4)!= -1 && cutM.at(i,2) == -1) )){
            allX.at(i,0) = 0;
            allY.at(i,0) = 0;
        }else{   // Both eyes were detected
            allX.at(i,0) = (cutM.at(i,2) + cutM.at(i,4))/2;  // x
            allY.at(i,0) = (cutM.at(i,3) + cutM.at(i,5))/2;  // y
        }
    }

    mat x1 = zeros(n_rows, n_rows);
    mat y1 = zeros(n_rows, n_rows);
    mat iw = zeros(n_rows, n_rows);
    mat iw2 = zeros(n_rows,n_rows);
    mat t1 = zeros(n_rows, n_rows);
    mat t2 = zeros(n_rows, n_rows);
    mat dw = zeros(n_rows, n_rows);
    mat tw = zeros(n_rows, n_rows);

    //old1:
    // [x1 x2] = meshgrid (allX , allX);
    //for (uword i = 0; i < n_rows; ++i){
    //     x1(span::all, i).fill(allX(i,0));  // Vertical columns with the data
    //     x2(i, span::all).fill(allX(i,0));  // Horizontal rows with the data.
    //}

    //new1:
    // [x1 x2] = meshgrid (allX , allX);
    for (uword i = 0; i < (uword)n_rows; ++i){
         x1(span::all, i).fill(allX(i,0));  // Vertical columns with the data
         y1(span::all, i).fill(allY(i,0));
         t1(span::all, i).fill(i);  // Vertical columns with the data
         //t2(i, span::all).fill(i);  // Horizontal rows with the data.
    }

    //x2 = x1.t();

    // iw = exp(-(x1-x2).^2./(2.*iscales.^2));
    iw = exp(-square(x1-x1.t()) / (2 * Consts::SMOOTH_ISCALES * Consts::SMOOTH_ISCALES));
    iw2 = exp(-square(y1-y1.t()) / (2 * Consts::SMOOTH_ISCALES * Consts::SMOOTH_ISCALES));

    // [t1 t2] = meshgrid((1:length(allxds)), (1:length(allxds)));
    dw = exp(-square(t1-t1.t()) / (2 * Consts::SMOOTH_SCALES * Consts::SMOOTH_SCALES));


    // tw = iwa.*iwa2.*iw .* iw2 .* dw;
    tw =  (iw % iw2) % dw;

    // Get only the ones that are not NaN
    uvec goodX =  arma::find(allX != datum::nan);
    uvec goodY =  arma::find(allY != datum::nan);

    mat smx = zeros(1,n_rows);
    smx.fill(datum::nan);

    mat smy = zeros(1,n_rows);
    smy.fill(datum::nan);

    smx.elem(goodX) = sum(((mat)(tw(goodX,goodX) % repmat(allX.elem(goodX),1, ((mat)allX.elem(goodX)).n_rows))).t(), 1) / sum( tw(goodX,goodX) , 1);
    smy.elem(goodY) = sum(((mat)(tw(goodY,goodY) % repmat(allY.elem(goodY),1, ((mat)allY.elem(goodY)).n_rows))).t(), 1) / sum( tw(goodY,goodY) , 1);

    smoothedM.col(2) = smx.t();
    smoothedM.col(3) = smy.t();
}

 mat GPMatrixFunctions::smoothSegment(mat &cutM, bool copy_eyes, int expWidth, int expHeight)
 {
     mat data = zeros(cutM.n_rows + 1, 4); // [miliseconds, 0, x, y]

     // When there are no values for one eye, copy the values from the other eye.
     // Calculate X and Y initial averages. Exclude when the eyes were not detected
     for (uword i = 0; i < cutM.n_rows; ++i){  // Eyes were not detected
         if (cutM(i,2) == -1 && cutM(i,4) == -1){
             data(i,2) = 0;
             data(i,3) = 0;
         }else if (copy_eyes && cutM(i,2)!= -1 && cutM(i,4) == -1 ) { // Left eyes were detected but not right
             data(i,2) = cutM(i,2) * expWidth;
             data(i,3) = cutM(i,3) * expHeight;
         }else if(copy_eyes && cutM(i,2) == -1 && cutM(i,4) != -1){  // Right eyes were detected, not left
             data(i,2) = cutM(i,4) * expWidth;
             data(i,3) = cutM(i,5) * expHeight;
         }else if(!copy_eyes && ((cutM(i,2)!= -1 && cutM(i,4) == -1) || (cutM(i,4)!= -1 && cutM(i,2) == -1) )){
             data(i,2) = 0;
             data(i,3) = 0;
         }else{   // Both eyes were detected
             data(i,2) = (cutM(i,2) + cutM(i,4))/2 * expWidth;  // x
             data(i,3) = (cutM(i,3) + cutM(i,5))/2  * expHeight;  // y
         }
     }

     mat allX =  data(span::all, 2);
     mat allY =  data(span::all, 3);

     mat x1 = zeros(allX.n_rows, allX.n_rows);
     mat x2 = zeros(allX.n_rows, allX.n_rows);
     mat iw = zeros(allX.n_rows, allX.n_rows);
     mat iw2 = zeros(allX.n_rows, allX.n_rows);
     mat t1 = zeros(allX.n_rows, allX.n_rows);
     mat t2 = zeros(allX.n_rows, allX.n_rows);
     mat dw = zeros(allX.n_rows, allX.n_rows);
     mat tw = zeros(allX.n_rows, allX.n_rows);

     // [x1 x2] = meshgrid (allX , allX);
     for (uword i = 0; i < allX.n_rows; ++i){
          x1(span::all, i).fill(allX(i,0));  // Vertical columns with the data
          x2(i, span::all).fill(allX(i,0));  // Horizontal rows with the data.
     }

     // iw = exp(-(x1-x2).^2./(2.*iscales.^2));
     iw = exp(-square(x1-x2) / (2 * Consts::SMOOTH_ISCALES * Consts::SMOOTH_ISCALES));

     for (uword i = 0; i < allY.n_rows; ++i){
          x1(span::all, i).fill(allY(i,0));  // Vertical columns with the data
          x2(i, span::all).fill(allY(i,0));  // Horizontal rows with the data.
     }
     iw2 = exp(-square(x1-x2) / (2 * Consts::SMOOTH_ISCALES * Consts::SMOOTH_ISCALES));

     // [t1 t2] = meshgrid((1:length(allxds)), (1:length(allxds)));
     for (uword i = 0; i < allX.n_rows; ++i){
          t1(span::all, i).fill(i);  // Vertical columns with the data
          t2(i, span::all).fill(i);  // Horizontal rows with the data.
     }
     dw = exp(-square(t1-t2) / (2 * Consts::SMOOTH_SCALES * Consts::SMOOTH_SCALES));


     // tw = iwa.*iwa2.*iw .* iw2 .* dw;
     tw =  (iw % iw2) % dw;

     // Get only the ones that are not NaN
     uvec goodX =  arma::find(allX != datum::nan);
     uvec goodY =  arma::find(allY != datum::nan);

     mat smx = zeros(1,allX.n_rows);
     smx.fill(datum::nan);

     mat smy = zeros(1,allY.n_rows);
     smy.fill(datum::nan);

     smx.elem(goodX) = sum(((mat)(tw(goodX,goodX) % repmat(allX.elem(goodX),1, ((mat)allX.elem(goodX)).n_rows))).t(), 1) / sum( tw(goodX,goodX) , 1);
     smy.elem(goodY) = sum(((mat)(tw(goodY,goodY) % repmat(allY.elem(goodY),1, ((mat)allY.elem(goodY)).n_rows))).t(), 1) / sum( tw(goodY,goodY) , 1);

     data.col(2) = smx.t();
     data.col(3) = smy.t();

     return data;

 }


// Root mean square (RMS)
// Will treat missing data as if its not there
double GPMatrixFunctions::fncCalculateRMSRough(mat &RoughM, int expWidth, int expHeight, double degPerPixel, bool copy_eyes) {

    if (RoughM.n_rows < 2 )
        return -1;

    mat RMS = mat(0, 2);
    uword validRow = 0;

    // process missing data, and get segments inbetween to smotth
    // [ leftx, lefty, rightx, righty]
    // Creates RMS matrix which has only non missing samples
    for (uword i = 0; i < RoughM.n_rows; ++i) {
        bool leftXMissing = (RoughM(i,2) < 0 || RoughM(i,2) > 1);
        bool leftYMissing = (RoughM(i,3) < 0 || RoughM(i,3) > 1);
        bool rightXMissing = (RoughM(i,4) < 0 || RoughM(i,4) > 1);
        bool rightYMissing = (RoughM(i,5) < 0 || RoughM(i,5) > 1);

        bool leftMissing = leftXMissing || leftYMissing;
        bool rightMissing = rightXMissing || rightYMissing;

        bool missing = copy_eyes ? (leftMissing && rightMissing) : (leftMissing || rightMissing);

        if (!missing) {
            RMS.insert_rows(validRow,1);

            RMS(validRow,0) = (RoughM(i,4) + RoughM(i,2)) / 2;
            RMS(validRow,1) = (RoughM(i,5) + RoughM(i,3)) / 2;

            if (copy_eyes) {
                if (leftMissing && !rightMissing) {
                    RMS(validRow,0) = RoughM(i,4);
                    RMS(validRow,1) = RoughM(i,5);
                } else if (rightMissing && !leftMissing) {
                    RMS(validRow,0) = RoughM(i,2);
                    RMS(validRow,1) = RoughM(i,3);
                }
            }
            validRow++;
        }
    }

    if (RMS.n_rows < 2 )
        return -1;

    // Calculate mean euclidean distance.
    mat squaredDistances = zeros(RMS.n_rows - 1);

    double x1, y1, x2, y2, xDiff, yDiff;

    double xMultiplier = expWidth * degPerPixel;
    double yMultiplier = expHeight * degPerPixel;

    x1 = RMS.at(0,0) * xMultiplier;
    y1 = RMS.at(0,1) * yMultiplier;

    for (uword j = 1; j < RMS.n_rows; ++j) {
        x2 = RMS.at(j,0) * xMultiplier;
        y2 = RMS.at(j,1) * yMultiplier;

        xDiff = x1 - x2;
        yDiff = y1 - y2;

        double distance = ((xDiff * xDiff) + (yDiff * yDiff));
        squaredDistances(j - 1) = distance * distance;

        x1 = x2;
        y1 = y2;
    }

    double rms = sqrt(mean(mean(squaredDistances)));
    qDebug() << " rms: " << rms;
    return rms;
}

 /*
  * Returns the mean euclidean distance from average of all points in passed array
  */
double GPMatrixFunctions::fncCalculateEuclideanDistanceSmooth(mat *p_a) {

    double xAvg = mean(p_a->col(0)); // x
    double yAvg = mean(p_a->col(1)); // y

    // Calculate mean euclidean distance.
    mat b = zeros(p_a->n_rows);
    for (uword j = 0; j < p_a->n_rows ; ++j) {
        double xDiffFromAvg = p_a->at(j,0) - xAvg;
        double yDiffFromAvg = p_a->at(j,1) - yAvg;
        b(j) =sqrt(((xDiffFromAvg * xDiffFromAvg) + ( yDiffFromAvg * yDiffFromAvg))/2);
    }
    return (mean(mean(b)));
}

 void GPMatrixFunctions::fncRemoveUndetectedValuesRough(mat *p_a){ // Removes the rows where any of the eyes were detected
     // Remove the rows where there was no valid data.
     // When there are no values for one eye, copy the values from the other eye.
     // Calculate X and Y initial averages. Exclude when the eyes were not detected
     for (uword i = 0; i < p_a->n_rows; ++i){  // Eyes were not detected
         if (p_a->at(i,0)!= -1 && p_a->at(i,2) == -1 ) { // Left eyes were detected but not right
             p_a->at(i,2) = p_a->at(i,0);
             p_a->at(i,3) = p_a->at(i,1);
         }else if(p_a->at(i,0) == -1 && p_a->at(i,2) != -1){  // Right eyes were detected, not left
             p_a->at(i,0) = p_a->at(i,2);
             p_a->at(i,1) = p_a->at(i,3);
         }
     }

     // Only use the points where eyes were detected:
     uvec fixIndex =  arma::find(p_a->col(0) != -1);
     if (!fixIndex.is_empty()){
         (*p_a) = p_a->rows(fixIndex);
     }

     if (p_a->n_rows == 0)
         (*p_a) = zeros(1,4);
 }


 /*
  *                                   Calculate Velocity
  * Takes 0 - 3 indexed columns of smooth matrix and returns 11 column matrix calculates velocity (col 4)
  * and whether it is a saccade (col 5).
  */
 void GPMatrixFunctions::fncCalculateVelocity(mat *p_smoothM, GrafixSettingsLoader settingsLoader)
 {
     int invalidSamples      = Consts::INVALID_SAMPLES;
     int expHeight           = settingsLoader.LoadSetting(Consts::SETTING_EXP_HEIGHT).toInt();
     int expWidth            = settingsLoader.LoadSetting(Consts::SETTING_EXP_WIDTH).toInt();
     double velocityThreshold= settingsLoader.LoadSetting(Consts::SETTING_VELOCITY_THRESHOLD).toDouble();
     double degreesPerPixel   = settingsLoader.LoadSetting(Consts::SETTING_DEGREE_PER_PIX).toDouble();
     int hz                  = settingsLoader.LoadSetting(Consts::SETTING_HZ).toInt();


     mat aux;
     double minC, maxX, maxY;
     double hzXdeg = hz * degreesPerPixel;

     // If SmoothM has less than 5 columns, create an 11 column version and copy over the first 4 columns.
     if (p_smoothM->n_cols < 5) {
         mat aux = zeros(p_smoothM->n_rows, 11);
         aux.cols(0,3) = p_smoothM->cols(0,3);
         (*p_smoothM) = aux;
     }

     // Iterate through each data point, add velocity at row 4, and whether it is saccade at row 5
     // invalidSamples is the number of samples to look backwards
     for (uword i = invalidSamples; i < p_smoothM->n_rows; ++i) {

         // Check if any of the coordinates for this or the previous samples were invalid
         aux = p_smoothM->submat(i - invalidSamples, 2, i, 3);  //( row1, col1, row2, col2)
         minC = aux.min();   //if any missing data minC will be -1
         maxX = aux.col(0).max();
         maxY = aux.col(1).max();

         if (minC > 0 && maxY < expWidth && maxX < expHeight) {

             // Calculate amplitude and velocity
             double xDist = (*p_smoothM)(i-1,2) - (*p_smoothM)(i,2);
             double yDist = (*p_smoothM)(i-1,3) - (*p_smoothM)(i,3);
             double amplitude = sqrt(((xDist * xDist) + (yDist * yDist)) / 2);
             double velocity = amplitude * hzXdeg;

             // Velocity
             p_smoothM->at(i,4) = velocity;

             // Saccade
             p_smoothM->at(i,5) = (velocity >= velocityThreshold ) ? 1 : 0;
         }
     }
 }

 void GPMatrixFunctions::fncCalculateFixations(mat *p_fixAllM, mat *p_roughM, mat *p_smoothM, GrafixSettingsLoader settingsLoader)
 {


     bool copy_eyes = settingsLoader.LoadSetting(Consts::SETTING_SMOOTHING_USE_OTHER_EYE).toBool();
     int expWidth            = settingsLoader.LoadSetting(Consts::SETTING_EXP_WIDTH).toInt();
     int expHeight           = settingsLoader.LoadSetting(Consts::SETTING_EXP_HEIGHT).toInt();
     double degreePerPixel   = settingsLoader.LoadSetting(Consts::SETTING_DEGREE_PER_PIX).toDouble();

     //clear fixall matrix
     (*p_fixAllM).reset();

     int indexStartFix = -1;
     for(uword i = 0; i < p_smoothM->n_rows-1; i ++){
         if (p_smoothM->at(i,5) == 1 ){ // Saccade. Velocity over threshold
             if (indexStartFix == -1 && p_smoothM->at(i + 1,5) == 0){ // The next point is the start of a fixation
                 indexStartFix = i;
             }else if (indexStartFix != -1 ){ // End of the fixation. Create fixation!

                 //duration is in ms
                 double dur = ((p_roughM->at(i,0) - p_roughM->at(indexStartFix,0)));

                 // Only use the points where eyes were detected:
                 mat roughCutM;
                 if (p_roughM->n_cols == 8){
                     roughCutM= p_roughM->submat(indexStartFix,2,i,7);
                     fncRemoveUndetectedValuesRough(&roughCutM);
                 }else{
                     roughCutM= p_roughM->submat(indexStartFix,2,i,5);
                     fncRemoveUndetectedValuesRough(&roughCutM);
                 }

                 double averageX = mean((roughCutM.col(0) + roughCutM.col(2))/2);
                 double averageY = mean((roughCutM.col(1) + roughCutM.col(3))/2);
                 double variance = fncCalculateRMSRough(roughCutM,expWidth,expHeight,degreePerPixel, copy_eyes);
                 double pupilDilation = 0;
                 if (roughCutM.n_cols == 6){
                     pupilDilation = (mean(roughCutM.col(4)) + mean(roughCutM.col(5)))/2;
                 }

                 mat row;
                 row << indexStartFix << i << dur << averageX << averageY << variance << 0 << pupilDilation <<  endr ;

                 if (p_fixAllM->n_rows == 0){
                     (*p_fixAllM) = row;
                 }else {
                     (*p_fixAllM) = join_cols((*p_fixAllM), row);
                 }

                 i = i - 1; // Otherwise it doesn't capture some fixations that as very close together
                 indexStartFix = -1;
             }
         }else if(indexStartFix != -1  && p_smoothM->at(i,2) < 1 && p_smoothM->at(i,3) < 1 ){ // Remove fake fixations: When the data disapears, the saccade flag does nt get activated and it creates fake fixations.
             indexStartFix = -1;
         }
     }
 }

 void GPMatrixFunctions::fncReturnFixationinSegments(mat *p_fixAllM, mat *p_segmentsM){
     mat fixations;

     if (!p_segmentsM->is_empty()){  // If there are experimental segments
         int fixIndex = 0;

         // order the segments
         uvec indices = sort_index(p_segmentsM->cols(1,1));

         mat a = p_segmentsM->rows(indices);

         for (uword i = 0; i < a.n_rows; ++i){  // Find fixations for each fragment and copy them in a matrix
             int start = a(i,1);
             int end = a(i,2);
             // Go through the whole fixations matrix
             for (uword j = fixIndex; j < p_fixAllM->n_rows; ++j){
                 if ((p_fixAllM->at(j,0) >= start && p_fixAllM->at(j,0) <= end )|| (p_fixAllM->at(j,0) < end && p_fixAllM->at(j,1) > end)){  // fixations that start  and end in the segment of start in the segment and end in the next
                     fixations = join_cols(fixations, p_fixAllM->row(j));
                 }
             }

         }
     }
     if (!fixations.is_empty())
         (*p_fixAllM) = fixations;//POSSIBLE ERROR?
 }

 void GPMatrixFunctions::fncRemoveMinFixations(mat *p_fixAllM, mat *p_smoothM, double minDur)
 {

     if (p_fixAllM->is_empty()) return; //cannot remove no fixations lol
     p_smoothM->col(9).fill(0); // Restart

     // Find all fixations we will delete
     uvec fixIndex =  arma::find(p_fixAllM->col(2) < minDur);
     mat minFix = p_fixAllM->rows(fixIndex);

     for (uword i = 0; i < minFix.n_rows; ++i){  // Modify the flag for the fixations we delete
         p_smoothM->operator()(span(minFix(i,0),minFix(i,1)),span(9,9) ).fill(1);
     }

     // Delete min fixations
     fixIndex =  arma::find(p_fixAllM->col(2) > minDur);

     if (!fixIndex.empty())
     {
         (*p_fixAllM) = p_fixAllM->rows(fixIndex);
     }
     else
     {
         (*p_fixAllM).reset();
     }

 }

 void GPMatrixFunctions::fncMergeDisplacementThreshold(mat *p_roughM, mat *p_smoothM, mat *p_fixAllM, GrafixSettingsLoader settingsLoader){
     if (p_fixAllM->n_rows < 3) return;

     //int invalidSamples      = MyConstants::INVALID_SAMPLES;
     int expWidth            = settingsLoader.LoadSetting(Consts::SETTING_EXP_WIDTH).toInt();
     int expHeight           = settingsLoader.LoadSetting(Consts::SETTING_EXP_HEIGHT).toInt();
     bool copy_eyes = settingsLoader.LoadSetting(Consts::SETTING_SMOOTHING_USE_OTHER_EYE).toBool();
     double degreePerPixel   = settingsLoader.LoadSetting(Consts::SETTING_DEGREE_PER_PIX).toDouble();
     int hz                  = settingsLoader.LoadSetting(Consts::SETTING_HZ).toInt();
     double displacement     = settingsLoader.LoadSetting(Consts::SETTING_POSTHOC_MERGE_CONSECUTIVE_VAL).toDouble();

     double dur, averageX, averageY, variance, pupilDilation, xDist1, yDist1,xDist2, yDist2;

     int gapLenght = 50 * hz / 1000; // to merge, the distance between one fixation and the next is less than 50 ms

     mat aux, roughCutM, f1,f2;

     uvec fixIndex;


     p_smoothM->col(7).fill(0); // Restart

     for (uword i = 0; i < p_fixAllM->n_rows-2; ++ i){

        // Recalculate euclidean distances for both fixations, using the smoothed data.
         aux =  p_smoothM->submat(p_fixAllM->at(i,0), 2, p_fixAllM->at(i,1) , 3 ); // cut the data for the next fixation to calculate euclidean distance
         fixIndex =  arma::find(aux.col(0) > -1); // Remove the -1 values for doing this calculation!!
         aux = aux.rows(fixIndex);
         if (!aux.is_empty()){
             xDist1 = mean(aux.col(0));
             yDist1 = mean(aux.col(1));
         }else{
             xDist1 = yDist1 = 1000;
         }

         aux =  p_smoothM->submat(p_fixAllM->at(i+1,0), 2, p_fixAllM->at(i+1,1) , 3 ); // cut the data for the next fixation to calculate euclidean distance
         fixIndex =  arma::find(aux.col(0) > -1); // Remove the -1 values for doing this calculation!!
         aux = aux.rows(fixIndex);
         if (!aux.is_empty()){
             xDist2 = mean(aux.col(0));
             yDist2 = mean(aux.col(1));
         }else{
             xDist2 = yDist2 = 0;
         }
         // Merge if: The displacement from fixation 1 and fixation 2 is less than "displacement" AND
         // if the distance between fixation 1 and fixation 2 is less than 50 ms .
         if (sqrt((xDist1 -  xDist2)* (xDist1 - xDist2)) * degreePerPixel <= displacement && sqrt((yDist1 -  yDist2)* (yDist1 - yDist2)) * degreePerPixel <= displacement  // if the euclidean distance between the two fixations is lower than the threshold, we merge!!
             && p_fixAllM->at(i+1,0) - p_fixAllM->at(i,1) <= gapLenght){
             // INdicate the flag
             p_smoothM->operator()(span(p_fixAllM->at(i,0),p_fixAllM->at(i+1,1)),span(7, 7) ).fill(1);

             // MERGE!
             dur = p_roughM->at(p_fixAllM->at(i+1,1),0)  - p_roughM->at(p_fixAllM->at(i,0),0);

             // Only use the points where eyes were detected:
             if (p_roughM->n_cols == 8){  // Depens on if we have pupil dilation data or not!
                 roughCutM= p_roughM->submat(p_fixAllM->at(i,0),2,p_fixAllM->at(i+1,1),7);
                 fncRemoveUndetectedValuesRough(&roughCutM);
             }else{
                 roughCutM= p_roughM->submat(p_fixAllM->at(i,0),2,p_fixAllM->at(i+1,1),5);
                 fncRemoveUndetectedValuesRough(&roughCutM);
             }

             averageX = mean((roughCutM.col(0) + roughCutM.col(2))/2);
             averageY = mean((roughCutM.col(1) + roughCutM.col(3))/2);
             variance = fncCalculateRMSRough(roughCutM, expWidth,expHeight,degreePerPixel,copy_eyes);
             pupilDilation = 0;
             if (roughCutM.n_cols == 6){
                 pupilDilation = (mean(roughCutM.col(4)) + mean(roughCutM.col(5)))/2;
             }

             p_fixAllM->at(i,1) = p_fixAllM->at(i+1,1);
             p_fixAllM->at(i,2) = dur;
             p_fixAllM->at(i,3) = averageX;
             p_fixAllM->at(i,4) = averageY;
             p_fixAllM->at(i,5) = variance;
             p_fixAllM->at(i,6) = 0;
             p_fixAllM->at(i,7) = pupilDilation;

             f1 = p_fixAllM->operator()( span(0, i), span(0, p_fixAllM->n_cols-1) );
             if (i + 1 < p_fixAllM->n_rows-1){
                 f2 = p_fixAllM->operator()(span(i+2,p_fixAllM->n_rows-1),span(0, p_fixAllM->n_cols-1) );
                 (*p_fixAllM) = join_cols(f1,f2);
                  i = i-1;
             }else{
                 (*p_fixAllM) = f1;
             }

         }
     }
 }

 void GPMatrixFunctions::fncRemoveHighVarianceFixations(mat *p_smoothM, mat *p_fixAllM, double variance){

     if (p_fixAllM->n_rows<1) return;
     p_smoothM->col(8).fill(0); // Restart

     for (uword i = 0; i < p_fixAllM->n_rows-1; ++i)
     {
         if (p_fixAllM->at(i,5) > variance)
         {
             // Remove fixation
             p_smoothM->operator()(span(p_fixAllM->at(i,0),p_fixAllM->at(i,1)),span(8,8) ).fill(1); // Indicate it
         }
     }

     uvec fixIndex =  arma::find(p_fixAllM->col(5) < variance);
     if (!fixIndex.empty())
     {
         (*p_fixAllM) = p_fixAllM->rows(fixIndex);
     }
     else
     {
         (*p_fixAllM).reset();
     }

 }

 bool GPMatrixFunctions::saveFileSafe(mat &matrix, QString fileName)
 {
     try
     {
         return saveFile(matrix,fileName);
     }
     catch(...)
     {
         return false;
     }
 }

 /*bool GPMatrixFunctions::saveFile(mat &matrix, QString fileName)
 {
     bool result;
     std::string filename = fileName.toStdString();
     try
     {
         result = matrix.save(filename, csv_ascii);
         if (!result) throw std::runtime_error("Error saving file: " + filename);
     }
     catch(...)
     {
         throw std::runtime_error("Error saving file: " + filename);
     }

     return result;
 }*/

bool GPMatrixFunctions::saveFile(mat &matrix, QString fileName)
{
    std::string filename = fileName.toStdString();
    std::ofstream stream(filename.c_str(),std::ios::binary);

    stream << std::fixed;

    uword nRows = matrix.n_rows;
    uword nCols = matrix.n_cols;
    if (!stream) return false;
    for (uword row = 0; row < nRows; ++row)
    {
        for (uword col = 0; col < (nCols-1); ++col)
        {
            stream << matrix.at(row,col);
            stream << ',';
        }
        stream << matrix.at(row,nCols-1);
        stream << endl;
    }
    stream.close();
    return true;
}
bool GPMatrixFunctions::readFileSafe(mat &matrix, QString fileName)
{
    try
    {
        return readFile(matrix,fileName);
    }
    catch(...)
    {
        return false;
    }
}

bool GPMatrixFunctions::readFile(mat &matrix, QString fileName)
{
     std::string filename = fileName.toStdString();

     double* squarematrix;
     unsigned char* buffer_;
     unsigned int buffer_size_;

     std::ifstream stream(filename.c_str(),std::ios::binary);

     if (stream) {

         stream.seekg (0,std::ios::end);
         buffer_size_ = static_cast<std::size_t>(stream.tellg());
         stream.seekg (0,std::ios::beg);

         if (0 != buffer_size_) {
             try {buffer_ = new unsigned char[buffer_size_];}
             catch (...){throw (std::runtime_error("Out of memory when loading: " + filename)); return false;}
             try {squarematrix = new double[buffer_size_/2];} //should have enough room
             catch (...){delete[] buffer_;throw (std::runtime_error("Out of memory when loading: " + filename)); return false;}
             unsigned int row = 0;
             unsigned int items = 0;

             try {
             stream.read(reinterpret_cast<char*>(buffer_),static_cast<std::streamsize>(buffer_size_));
             qDebug() << QString::fromStdString(filename) + " -Loaded";

             bool found_a_number = false;
             //parse
             for (unsigned int pos = 0; pos<buffer_size_; ++pos) {
                 if (_want_to_close) return false;
                 switch (buffer_[pos]) {
                 case ',':
                     found_a_number=false;
                     break;
                 case '\n':
                     found_a_number=false;
                     ++row;
                     break;
                 case '0': case '1':case '2': case '3': case '4':
                 case '5': case '6': case '7': case '8': case '9':
                 case '-': case '+':
                     if (!found_a_number) {  //start of a string
                         unsigned int move_on=0;
                         squarematrix[items++] = GPGeneralStaticLibrary::string_to_double_fast(buffer_+pos,move_on);
                         pos += move_on;
                         found_a_number=true;
                     }
                     break;
                 default:  //whitespace
                     break;
                 }
             }

             unsigned int n_cols = items / row;
             matrix = mat(squarematrix,n_cols,row).t();
                 } catch (...) {
                     delete[] buffer_;
                     delete[] squarematrix;
                     buffer_ = 0;
                     squarematrix = 0;
                     qDebug() << "Error loading file";
                     throw (std::runtime_error("Error parsing CSV: " + filename));
                     return false;
                 }
             delete[] buffer_;
             delete[] squarematrix;
             buffer_ = 0;
             squarematrix = 0;
         }
         stream.close();
         return true;
     }
     throw (std::runtime_error("Error loading: " + filename));
     return false;
}

int GPMatrixFunctions::fncGetMatrixColsFromFile(QString fullpath)
{
    std::string filename = fullpath.toStdString();

    unsigned char* buffer_;
    unsigned int buffer_size_;
    int n_cols = 0;

    std::ifstream stream(filename.c_str(),std::ios::binary);

    if (stream) {

        stream.seekg (0,std::ios::end);
        buffer_size_ = static_cast<std::size_t>(stream.tellg());
        stream.seekg (0,std::ios::beg);

        if (0 != buffer_size_) {
            try {buffer_ = new unsigned char[buffer_size_];}
            catch (...){return 0;}

            try {
            stream.read(reinterpret_cast<char*>(buffer_),static_cast<std::streamsize>(buffer_size_));
            qDebug() << QString::fromStdString(filename) + " -Loaded";

            bool found_a_number = false;
            bool found_a_row = false;
            unsigned int pos = 0;
            //parse
            while (!found_a_row && pos<buffer_size_)
            {
                switch (buffer_[pos])
                {
                case ',':
                    n_cols++;
                    found_a_number=false;
                    break;
                case '\n':
                    n_cols++;
                    found_a_row = true;
                    found_a_number=false;
                    break;
                case '0': case '1':case '2': case '3': case '4':
                case '5': case '6': case '7': case '8': case '9':
                case '-': case '+':
                    if (!found_a_number) {  //start of a string
                        unsigned int move_on=0;
                        GPGeneralStaticLibrary::string_to_double_fast(buffer_+pos,move_on);
                        pos += move_on;
                        found_a_number=true;
                    }
                    break;
                default:  //whitespace
                    break;
                }
                ++pos;
            }

            } catch (...) {
                delete[] buffer_;
                buffer_ = 0;
                qDebug() << "Error loading file";
                return 0;
            }
            delete[] buffer_;
            buffer_ = 0;
        }
        stream.close();
        return n_cols;
    }
    return 0;
}





