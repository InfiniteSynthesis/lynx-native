// Copyright 2017 The Lynx Authors. All rights reserved.

#ifndef LYNX_LAYOUT_CSS_TYPE_H_
#define LYNX_LAYOUT_CSS_TYPE_H_

#include <limits.h>
#include <string>

namespace lynx {

#define CSS_UNDEFINED 0x7FFFFFFF
#define CSS_MAX (0x7FFFFFFF - 1)
#define CSS_IS_UNDEFINED(VAL) (VAL == INT_MAX)

enum CSSStyleType {
  CSS_DISPLAY_NONE = 0,
  CSS_DISPLAY_FLEX = 1,

  CSSFLEX_ALIGN_AUTO = 2,
  CSSFLEX_ALIGN_FLEX_START = 3,
  CSSFLEX_ALIGN_CENTER = 4,
  CSSFLEX_ALIGN_FLEX_END = 5,
  CSSFLEX_ALIGN_STRETCH = 6,

  CSSFLEX_DIRECTION_COLUMN = 7,
  CSSFLEX_DIRECTION_COLUMN_REVERSE = 8,
  CSSFLEX_DIRECTION_ROW = 9,
  CSSFLEX_DIRECTION_ROW_REVERSE = 10,

  CSSFLEX_JUSTIFY_FLEX_START = 11,
  CSSFLEX_JUSTIFY_FLEX_CENTER = 12,
  CSSFLEX_JUSTIFY_FLEX_END = 13,
  CSSFLEX_JUSTIFY_SPACE_BETWEEN = 14,
  CSSFLEX_JUSTIFY_SPACE_AROUND = 15,

  CSSFLEX_WRAP = 16,
  CSSFLEX_NOWRAP = 17,
  CSSFLEX_WRAP_REVERSE = 18,

  CSSFLEX_ORDER = 19,

  CSS_POSITION_RELATIVE = 20,
  CSS_POSITION_ABSOLUTE = 21,
  CSS_POSITION_FIXED = 22,

  CSS_VISIBLE = 23,
  CSS_HIDDEN = 24,

  CSS_POINTER_EVENTS_NONE = 25,
  CSS_POINTER_EVENTS_AUTO = 26,

  CSS_BACKGROUND_REPEAT = 27,
  CSS_BACKGROUND_REPEAT_X = 28,
  CSS_BACKGROUND_REPEAT_Y = 29,
  CSS_BACKGROUND_NO_REPEAT = 30
};

enum TextStyleType {
  CSSTEXT_ALIGN_LEFT = 0,
  CSSTEXT_ALIGN_RIGHT,
  CSSTEXT_ALIGN_CENTER,

  CSSTEXT_TEXTDECORATION_LINETHROUGH,
  CSSTEXT_TEXTDECORATION_NONE,

  CSSTEXT_FONT_WEIGHT_NORMAL,
  CSSTEXT_FONT_WEIGHT_BOLD,

  CSSTEXT_OVERFLOW_ELLIPSIS,
  CSSTEXT_OVERFLOW_NONE,

  CSSTEXT_WHITESPACE_NOWRAP,
  CSSTEXT_WHITESPACE_NORMAL
};

enum ImageStyleType {
  CSSIMAGE_OBJECT_FIT_FILL = 0,
  CSSIMAGE_OBJECT_FIT_CONTAIN,
  CSSIMAGE_OBJECT_FIT_COVER
};

bool ToVisibleType(const std::string& value, CSSStyleType& type);

bool ToDisplayType(const std::string& value, CSSStyleType& type);

bool ToFlexAlignType(const std::string& value, CSSStyleType& type);

bool ToFlexDirectionType(const std::string& value, CSSStyleType& type);

bool ToFlexJustifyType(const std::string& value, CSSStyleType& type);

bool ToFlexWrapType(const std::string& value, CSSStyleType& type);

bool ToFlexOrder(const std::string& value, CSSStyleType& type);

bool ToPositionType(const std::string& value, CSSStyleType& type);

bool ToPointerEventsType(const std::string& value, CSSStyleType & type);

bool ToBackgroundImageRepeatType(const std::string &value, CSSStyleType &type);

bool ToTextAlignType(const std::string& value, TextStyleType& type);

bool ToTextDecorationType(const std::string& value, TextStyleType& type);

bool ToTextFontWeightType(const std::string& value, TextStyleType& type);

bool ToTextOverflowType(const std::string& value, TextStyleType& type);

bool ToTextWhiteSpaceType(const std::string& value, TextStyleType& type);

bool ToObjectFitType(const std::string& value, ImageStyleType& type);

std::string MapCSSType(CSSStyleType type);
std::string MapTextStyle(TextStyleType type);
std::string MapImageStyle(ImageStyleType type);

}  // namespace lynx

#endif  // LYNX_LAYOUT_CSS_TYPE_H_
