To install in R:
```
library(devtools)
devtools::install_github("ungil/Cairo")
```

Example of use:
```
(ql:quickload :rcl)

(in-package :rcl)

(ensure-r '("Cairo"))

(cffi:load-foreign-library (format nil "~A/libs/i386/Cairo.dll" (first (r:r "find.package" "Cairo"))))

(fli:define-foreign-variable (backend-pointer "backend") :type (:pointer :void))

(fli:define-foreign-function (backend-resize "backend_resize_lw")
    ((be :pointer) (width :double) (height :double))
  :result-type :unsigned-int :calling-convention :stdcall)

(defclass cairo-pane (capi:output-pane)
  ((backend :accessor backend :initform nil)
   (device :accessor device :initform nil)))

(defmethod initialize-instance :after ((cairo-pane cairo-pane) &key)
  (setf (slot-value cairo-pane 'capi::create-callback) #'open-cairo
        (slot-value cairo-pane 'capi::destroy-callback) #'close-cairo
        (slot-value cairo-pane 'capi::resize-callback) #'display-cairo))

(defmethod open-cairo ((cairo-pane cairo-pane))
  (setf (device cairo-pane) (first (r:r "CairoLW" :hwnd (capi:simple-pane-handle cairo-pane)
					:width (capi:simple-pane-visible-width cairo-pane) 
					:height (capi:simple-pane-visible-height cairo-pane))))
  (setf (backend cairo-pane) (backend-pointer)))

(defmethod close-cairo ((cairo-pane cairo-pane))
  (when (device cairo-pane)
    (r:r "dev.off" (device cairo-pane))))

(defmethod display-cairo ((cairo-pane cairo-pane) x y width height)
  (when (backend cairo-pane)
    (backend-resize (backend cairo-pane) (* 1d0 width) (* 1d0 height))))

(defmethod set-current-device ((cairo-pane cairo-pane))
  (if (device cairo-pane)
      (r:r "dev.set" (device cairo-pane))
    (open-cairo cairo-pane)))

(defmacro with-capi-device (cairo-pane &body body)
  `(progn (set-current-device ,cairo-pane)
     ,@body
     (capi:with-geometry ,cairo-pane 
       (display-cairo ,cairo-pane 0 0 capi:%width% capi:%height%))))

(r "library" "stats")

(defvar *dist* (r%-parse-eval "dist(USArrests)"))

(defun plot-cluster (method cairo-demo)
 (with-slots (cairo) cairo-demo
   (with-capi-device cairo
     (r% "plot" (r% "hclust" *dist* :method method)
         :main (format nil "USA Arrests - Method: ~A" method) :xlab "" :ylab "" :sub ""))))

(capi:define-interface cairo-demo ()
  ()
  (:panes 
   (method-choice capi:push-button-panel :interaction :single-selection
                  :title "Select agglomeration method:" :title-position :left
                  :items '("ward.D" "ward.D2" "single" "complete" "average" "mcquitty" "median" "centroid")
                  :test-function 'equalp :selection-callback 'plot-cluster)
   (cairo cairo-pane))
  (:default-initargs :title "RCL Cairo Test" :best-width 800 :best-height 500))

(capi:display (make-instance 'cairo-demo))
```

The following demo executable should work if a standard installation of R 3.4.3 is present: http://www.ungil.com/rcl-capi-demo.exe