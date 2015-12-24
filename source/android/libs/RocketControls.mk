LOCAL_PATH := $(QFUSION_PATH)/libsrcs/libRocket/libRocket
include $(CLEAR_VARS)
LOCAL_MODULE := RocketControls

LOCAL_EXPORT_C_INCLUDES := $(LOCAL_PATH)/include
LOCAL_C_INCLUDES := $(LOCAL_EXPORT_C_INCLUDES)

LOCAL_SRC_FILES := \
  Source/Controls/Clipboard.cpp \
  Source/Controls/Controls.cpp \
  Source/Controls/DataFormatter.cpp \
  Source/Controls/DataQuery.cpp \
  Source/Controls/DataSource.cpp \
  Source/Controls/DataSourceListener.cpp \
  Source/Controls/ElementDataGrid.cpp \
  Source/Controls/ElementDataGridCell.cpp \
  Source/Controls/ElementDataGridExpandButton.cpp \
  Source/Controls/ElementDataGridRow.cpp \
  Source/Controls/ElementForm.cpp \
  Source/Controls/ElementFormControl.cpp \
  Source/Controls/ElementFormControlDataSelect.cpp \
  Source/Controls/ElementFormControlInput.cpp \
  Source/Controls/ElementFormControlSelect.cpp \
  Source/Controls/ElementFormControlTextArea.cpp \
  Source/Controls/ElementTabSet.cpp \
  Source/Controls/ElementTextSelection.cpp \
  Source/Controls/InputType.cpp \
  Source/Controls/InputTypeButton.cpp \
  Source/Controls/InputTypeCheckbox.cpp \
  Source/Controls/InputTypeRadio.cpp \
  Source/Controls/InputTypeRange.cpp \
  Source/Controls/InputTypeSubmit.cpp \
  Source/Controls/InputTypeText.cpp \
  Source/Controls/SelectOption.cpp \
  Source/Controls/WidgetDropDown.cpp \
  Source/Controls/WidgetSlider.cpp \
  Source/Controls/WidgetSliderInput.cpp \
  Source/Controls/WidgetTextInput.cpp \
  Source/Controls/WidgetTextInputMultiLine.cpp \
  Source/Controls/WidgetTextInputSingleLine.cpp \
  Source/Controls/WidgetTextInputSingleLinePassword.cpp \
  Source/Controls/XMLNodeHandlerDataGrid.cpp \
  Source/Controls/XMLNodeHandlerTabSet.cpp \
  Source/Controls/XMLNodeHandlerTextArea.cpp

include $(BUILD_STATIC_LIBRARY)
