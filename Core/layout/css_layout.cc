// Copyright 2017 The Lynx Authors. All rights reserved.

#include "layout/css_layout.h"
#include <algorithm>
#include <vector>
#include "layout/css_style.h"
#include "layout/css_type.h"
#include "layout/layout_object.h"
#ifndef TESTING
#include "render/render_object.h"
#include "render/render_tree_host.h"
#endif
using namespace std;

namespace lynx {

//实现order排序的比较函数
bool CompareFlexOrder(LayoutObject* obj1, LayoutObject* obj2) {
  const CSSStyle* item1_style = &(obj1->css_style());
  const CSSStyle* item2_style = &(obj2->css_style());
  return item1_style->flex_order_ < item2_style->flex_order_;
}

base::Size CSSStaticLayout::Measure(LayoutObject* renderer,
                                    int width_descriptor,
                                    int height_descriptor) {
  const CSSStyle* style = &(renderer->css_style());

  int width_mode = base::Size::Descriptor::GetMode(width_descriptor);
  int height_mode = base::Size::Descriptor::GetMode(height_descriptor);

  int measured_width_mode = width_mode;
  int measured_height_mode = height_mode;
  if (!CSS_IS_UNDEFINED(style->width_) ||
      !CSS_IS_UNDEFINED(style->max_width_)) {
    measured_width_mode = base::Size::Descriptor::EXACTLY;
  }
  if (!CSS_IS_UNDEFINED(style->height_) ||
      !CSS_IS_UNDEFINED(style->max_height_)) {
    measured_height_mode = base::Size::Descriptor::EXACTLY;
  }

  int measured_width = base::Size::Descriptor::GetSize(width_descriptor);
  int measured_height = base::Size::Descriptor::GetSize(height_descriptor);
  if (measured_width_mode != base::Size::Descriptor::UNSPECIFIED) {
    measured_width =
        style->ClampWidth(base::Size::Descriptor::GetSize(width_descriptor));
  }
  if (measured_height_mode != base::Size::Descriptor::UNSPECIFIED) {
    measured_height =
        style->ClampHeight(base::Size::Descriptor::GetSize(height_descriptor));
  }

  base::Size size = MeasureInner(renderer, measured_width, measured_width_mode,
                                 measured_height, measured_height_mode);

  int w = !CSS_IS_UNDEFINED(width_descriptor) &&
                  width_mode == base::Size::Descriptor::EXACTLY
              ? style->ClampExactWidth(
                    base::Size::Descriptor::GetSize(width_descriptor))
              : style->ClampWidth(size.width_);
  int h = !CSS_IS_UNDEFINED(height_descriptor) &&
                  height_mode == base::Size::Descriptor::EXACTLY
              ? style->ClampExactHeight(
                    base::Size::Descriptor::GetSize(height_descriptor))
              : style->ClampHeight(size.height_);

  size.Update(w, h);
  return size;
}

base::Size CSSStaticLayout::MeasureInner(LayoutObject* renderer,
                                         int width,
                                         int width_mode,
                                         int height,
                                         int height_mode) {
  base::Size measured_size;
  const CSSStyle* style = &(renderer->css_style());

  int w = width;
  int h = height;

  int available_width = w - style->padding_left_ - style->padding_right_ -
                        style->border_width_ * 2;
  int available_height = h - style->padding_top_ - style->padding_bottom_ -
                         style->border_width_ * 2;

  if (style->flex_direction_ == CSSFLEX_DIRECTION_ROW ||
      style->flex_direction_ == CSSFLEX_DIRECTION_ROW_REVERSE) {
    measured_size = MeasureRow(renderer, available_width, width_mode,
                               available_height, height_mode);
  } else {
    // measure
    measured_size = MeasureColumn(renderer, available_width, width_mode,
                                  available_height, height_mode);
  }

  w = measured_size.width_ + style->padding_left_ + style->padding_right_ +
      style->border_width_ * 2;
  h = measured_size.height_ + style->padding_top_ + style->padding_bottom_ +
      style->border_width_ * 2;

  measured_size.Update(w, h);
  return measured_size;
}

bool CSSStaticLayout::MeasureSpecially(LayoutObject* renderer,
                                       int width,
                                       int width_mode,
                                       int height,
                                       int height_mode) {
  const CSSStyle* child_style = &(renderer->css_style());
  if (child_style->css_display_type_ != CSS_DISPLAY_FLEX) {
    renderer->Measure(0, 0);
    return true;
  }
  if (child_style->css_position_type_ == CSS_POSITION_ABSOLUTE) {
    MeasureAbsolute(renderer, width, width_mode, height, height_mode);
    return true;
  }
  if (child_style->css_position_type_ == CSS_POSITION_FIXED) {
    MeasureFixed(renderer);
    return true;
  }
  return false;
}

base::Size CSSStaticLayout::MeasureRowOneLine(LayoutObject* renderer,
                                              int width,
                                              int width_mode,
                                              int height,
                                              int height_mode,
                                              int start,
                                              int end) {
  base::Size measured_size(0, 0);
  int calc_width = 0;
  int calc_height = 0;

  float total_flex = 0;
  // 对于row排版，且是no_wrap的操作，孩子想要多宽就给多宽
  float residual_width = width;

  //计算子view中无flex属性的view
  // size，并且计算出剩下含有flex属性的子view可使用的宽度
  for (int i = start; i <= end; i++) {
    int measure_width;

    LayoutObject* child = (LayoutObject*)renderer->Find(i);
    const CSSStyle* child_style = &(child->css_style());

    if (MeasureSpecially(child, width, width_mode, height, height_mode)) {
      continue;
    }

    if (child_style->flex_ > 0) {
      total_flex += child_style->flex_;
      calc_width += child_style->margin_left_ + child_style->margin_right_;
      continue;
    }

    if (!CSS_IS_UNDEFINED(child_style->width_)) {
      measure_width = base::Size::Descriptor::Make(
          child_style->ClampWidth(), base::Size::Descriptor::AT_MOST);
    } else {
      measure_width = base::Size::Descriptor::Make(
          residual_width - child_style->margin_left_ -
              child_style->margin_right_,
          base::Size::Descriptor::UNSPECIFIED);
    }

    base::Size child_size = child->Measure(
        measure_width,
        base::Size::Descriptor::Make(
            height - child_style->margin_top_ - child_style->margin_bottom_,
            base::Size::Descriptor::AT_MOST));

    calc_width += child_size.width_ + child_style->margin_left_ +
                  child_style->margin_right_;
    calc_height = child_size.height_;

    int child_item_height =
        calc_height + child_style->margin_bottom_ + child_style->margin_top_;
    measured_size.height_ = measured_size.height_ > child_item_height
                                ? measured_size.height_
                                : child_item_height;
  }

  if ((calc_width == 0 && total_flex > 0) ||
      (CSS_IS_UNDEFINED(renderer->css_style().width_) &&
       CSS_IS_UNDEFINED(renderer->css_style().max_width_) &&
       renderer->css_style().min_width_ == 0)) {
    residual_width = renderer->css_style().ClampWidth(width);
    if (residual_width != width) {
      residual_width = residual_width - renderer->css_style().padding_right_ -
                       renderer->css_style().padding_left_ -
                       2 * renderer->css_style().border_width_;
    }
    residual_width -= calc_width;
  } else {
    residual_width = renderer->css_style().ClampWidth(calc_width) -
                     renderer->css_style().padding_right_ -
                     renderer->css_style().padding_left_ -
                     2 * renderer->css_style().border_width_ - calc_width;
  }

  if (residual_width < 0)
    residual_width = 0;

  for (int i = start; i <= end; i++) {
    LayoutObject* child = (LayoutObject*)renderer->Find(i);
    const CSSStyle* child_style = &(child->css_style());

    if (child_style->css_display_type_ != CSS_DISPLAY_FLEX) {
      continue;
    }
    if (child_style->css_position_type_ == CSS_POSITION_ABSOLUTE ||
        child_style->css_position_type_ == CSS_POSITION_FIXED) {
      continue;
    }

    if (child_style->flex_ <= 0)
      continue;

    //根据flex属性重新计算子view宽度
    int recalc_width = round(residual_width * child_style->flex_ / total_flex);

    int measure_width =
        recalc_width == 0
            ? base::Size::Descriptor::Make(width,
                                           base::Size::Descriptor::AT_MOST)
            : width_mode == base::Size::Descriptor::UNSPECIFIED
                  ? base::Size::Descriptor::Make(
                        recalc_width, base::Size::Descriptor::AT_MOST)
                  : base::Size::Descriptor::Make(
                        recalc_width, base::Size::Descriptor::EXACTLY);

    //测量子view的宽高/
    base::Size child_size = child->Measure(
        measure_width,
        base::Size::Descriptor::Make(height, base::Size::Descriptor::AT_MOST));

    calc_width += child_size.width_;
    calc_height = child_size.height_;

    int child_item_height =
        calc_height + child_style->margin_bottom_ + child_style->margin_top_;
    measured_size.height_ = measured_size.height_ > child_item_height
                                ? measured_size.height_
                                : child_item_height;
    residual_width -= child_size.width_;
    total_flex -= child_style->flex_;
  }
  measured_size.width_ = calc_width;
  // measured_size.height_ = calc_height;
  return measured_size;
}

base::Size CSSStaticLayout::MeasureRowWrap(LayoutObject* renderer,
                                           int width,
                                           int width_mode,
                                           int height,
                                           int height_mode) {
  int current_calc_width = 0;
  int calc_width = 0;
  int calc_height = 0;
  int start = 0;

  base::Size measured_size(renderer->css_style().width_,
                           renderer->css_style().height_);

  for (int index = 0, child_view_count = renderer->GetChildCount();
       index < child_view_count;) {
    LayoutObject* child = (LayoutObject*)renderer->Find(index);
    const CSSStyle* child_style = &(child->css_style());

    if (MeasureSpecially(child, width, width_mode, height, height_mode)) {
      index++;
      continue;
    }

    int measure_width =
        width - child_style->margin_left_ - child_style->margin_right_;
    base::Size child_size(0, 0);

    if (child_style->flex_ == 0) {
      child_size =
          child->Measure(base::Size::Descriptor::Make(
                             measure_width, base::Size::Descriptor::AT_MOST),
                         base::Size::Descriptor::Make(
                             height, base::Size::Descriptor::AT_MOST));
    }

    current_calc_width = current_calc_width + child_size.width_ +
                         child_style->margin_left_ + child_style->margin_right_;
    if (current_calc_width <= width) {
      calc_width = current_calc_width;
      if (index == child_view_count - 1) {
        base::Size row_size = MeasureRowOneLine(
            renderer, width, width_mode, height, height_mode, start, index);
        calc_width =
            row_size.width_ > calc_width ? row_size.width_ : calc_width;
        calc_height += row_size.height_;
      }
      index++;
    }

    // 当前行宽度占满，准备往下排时，或者是已经排到了最后一行的最后一个元素
    else {
      int end = index;
      if (start != index) {
        end -= 1;
      }

      base::Size row_size = MeasureRowOneLine(renderer, width, width_mode,
                                              height, height_mode, start, end);

      calc_width = row_size.width_ > calc_width ? row_size.width_ : calc_width;
      calc_height += row_size.height_;
      if (start == index) {
        index += 1;
      }
      start = index;
      current_calc_width = 0;
    }
  }
  measured_size.width_ = calc_width;
  measured_size.height_ = calc_height;
  return measured_size;
}

base::Size CSSStaticLayout::MeasureRow(LayoutObject* renderer,
                                       int width,
                                       int width_mode,
                                       int height,
                                       int height_mode) {
  const CSSStyle* item_style = &(renderer->css_style());
  if (item_style->flex_wrap_ != CSSFLEX_WRAP) {
    return MeasureRowOneLine(renderer, width, width_mode, height, height_mode,
                             0, renderer->GetChildCount() - 1);
  } else {
    // measure flex-wrap
    return MeasureRowWrap(renderer, width, width_mode, height, height_mode);
  }
}

base::Size CSSStaticLayout::MeasureColumnOneLine(LayoutObject* renderer,
                                                 int width,
                                                 int width_mode,
                                                 int height,
                                                 int height_mode,
                                                 int start,
                                                 int end) {
  base::Size measured_size(0, 0);
  int calc_width = 0;
  int calc_height = 0;

  float total_flex = 0;
  // 对于row排版，且是no_wrap的操作，孩子想要多宽就给多宽
  float residual_height = height;

  //计算子view中无flex属性的view
  // size，并且计算出剩下含有flex属性的子view可使用的宽度
  for (int i = start; i <= end; i++) {
    int measure_height;

    LayoutObject* child = (LayoutObject*)renderer->Find(i);
    const CSSStyle* child_style = &(child->css_style());

    if (MeasureSpecially(child, width, width_mode, height, height_mode)) {
      continue;
    }

    if (child_style->flex_ > 0) {
      total_flex += child_style->flex_;
      calc_height += child_style->margin_top_ + child_style->margin_bottom_;
      continue;
    }

    if (!CSS_IS_UNDEFINED(child_style->height_)) {
      measure_height = base::Size::Descriptor::Make(
          child_style->ClampHeight(), base::Size::Descriptor::AT_MOST);
    } else {
      measure_height = base::Size::Descriptor::Make(
          residual_height - calc_height - child_style->margin_top_ -
              child_style->margin_bottom_,
          base::Size::Descriptor::UNSPECIFIED);
    }

    base::Size child_size = child->Measure(
        base::Size::Descriptor::Make(
            width - child_style->margin_right_ - child_style->margin_left_,
            base::Size::Descriptor::AT_MOST),
        measure_height);

    calc_width = child_size.width_;
    calc_height += child_size.height_ + child_style->margin_top_ +
                   child_style->margin_bottom_;

    int child_item_width =
        calc_width + child_style->margin_left_ + child_style->margin_right_;
    measured_size.width_ = measured_size.width_ > child_item_width
                               ? measured_size.width_
                               : child_item_width;
  }

  if ((calc_height == 0 && total_flex > 0) ||
      (CSS_IS_UNDEFINED(renderer->css_style().height_) &&
       CSS_IS_UNDEFINED(renderer->css_style().max_height_) &&
       renderer->css_style().min_height_ == 0)) {
    residual_height = renderer->css_style().ClampHeight(height);
    if (residual_height != height) {
      residual_height = residual_height - renderer->css_style().padding_top_ -
                        renderer->css_style().padding_bottom_ -
                        2 * renderer->css_style().border_width_;
    }
    residual_height -= calc_height;
  } else {
    residual_height = renderer->css_style().ClampHeight(calc_height) -
                      renderer->css_style().padding_top_ -
                      renderer->css_style().padding_bottom_ -
                      2 * renderer->css_style().border_width_ - calc_height;
  }

  if (residual_height < 0)
    residual_height = 0;

  for (int i = start; i <= end; i++) {
    LayoutObject* child = (LayoutObject*)renderer->Find(i);
    const CSSStyle* child_style = &(child->css_style());

    if (child_style->css_display_type_ != CSS_DISPLAY_FLEX) {
      continue;
    }
    if (child_style->css_position_type_ == CSS_POSITION_ABSOLUTE ||
        child_style->css_position_type_ == CSS_POSITION_FIXED) {
      continue;
    }

    if (child_style->flex_ <= 0)
      continue;

    //根据flex属性重新计算子view宽度
    int recalc_height =
        round(residual_height * child_style->flex_ / total_flex);

    //测量子view的宽高/
    int measure_height =
        recalc_height == 0
            ? base::Size::Descriptor::Make(height,
                                           base::Size::Descriptor::AT_MOST)
            : height_mode == base::Size::Descriptor::UNSPECIFIED
                  ? base::Size::Descriptor::Make(
                        recalc_height, base::Size::Descriptor::AT_MOST)
                  : base::Size::Descriptor::Make(
                        recalc_height, base::Size::Descriptor::EXACTLY);

    base::Size child_size = child->Measure(
        base::Size::Descriptor::Make(width, base::Size::Descriptor::AT_MOST),
        measure_height);

    calc_width = child_size.width_;
    calc_height += child_size.height_;

    int child_item_width =
        calc_width + child_style->margin_left_ + child_style->margin_right_;
    measured_size.width_ = measured_size.width_ > child_item_width
                               ? measured_size.width_
                               : child_item_width;
    residual_height -= child_size.height_;
    total_flex -= child_style->flex_;
  }
  // measured_size.width_ = calc_width;
  measured_size.height_ = calc_height;
  return measured_size;
}

base::Size CSSStaticLayout::MeasureColumnWrap(LayoutObject* renderer,
                                              int width,
                                              int width_mode,
                                              int height,
                                              int height_mode) {
  int current_calc_height = 0;
  int calc_width = 0;
  int calc_height = 0;
  int start = 0;

  base::Size measured_size(renderer->css_style().width_,
                           renderer->css_style().height_);

  for (int index = 0, child_view_count = renderer->GetChildCount();
       index < child_view_count;) {
    LayoutObject* child = (LayoutObject*)renderer->Find(index);
    const CSSStyle* child_style = &(child->css_style());

    if (MeasureSpecially(child, width, width_mode, height, height_mode)) {
      index++;
      continue;
    }

    int measure_height =
        height - child_style->margin_top_ - child_style->margin_bottom_;
    base::Size child_size(0, 0);

    if (child_style->flex_ == 0) {
      child_size = child->Measure(
          base::Size::Descriptor::Make(width, base::Size::Descriptor::AT_MOST),
          base::Size::Descriptor::Make(measure_height,
                                       base::Size::Descriptor::AT_MOST));
    }

    current_calc_height = current_calc_height + child_size.height_ +
                          child_style->margin_top_ +
                          child_style->margin_bottom_;
    if (current_calc_height <= height) {
      calc_height = current_calc_height;
      if (index == child_view_count - 1) {
        base::Size column_size = MeasureColumnOneLine(
            renderer, width, width_mode, height, height_mode, start, index);

        calc_width += column_size.width_;
        calc_height = column_size.height_ > calc_height ? column_size.height_
                                                        : calc_height;
      }
      index++;
    }

    // 当前行宽度占满，准备往下排时，或者是已经排到了最后一行的最后一个元素
    else {
      int end = index;
      if (start != index) {
        end -= 1;
      }

      base::Size column_size = MeasureColumnOneLine(
          renderer, width, width_mode, height, height_mode, start, end);

      calc_width += column_size.width_;
      calc_height =
          column_size.height_ > calc_height ? column_size.height_ : calc_height;

      if (start == index) {
        index += 1;
      }
      start = index;
      // current_calc_height = 0;
    }
  }
  measured_size.width_ = calc_width;
  measured_size.height_ = calc_height;
  return measured_size;
}

base::Size CSSStaticLayout::MeasureColumn(LayoutObject* renderer,
                                          int width,
                                          int width_mode,
                                          int height,
                                          int height_mode) {
  const CSSStyle* item_style = &(renderer->css_style());
  if (item_style->flex_wrap_ != CSSFLEX_WRAP) {
    return MeasureColumnOneLine(renderer, width, width_mode, height,
                                height_mode, 0, renderer->GetChildCount() - 1);
  } else {
    // measure flex-wrap
    return MeasureColumnWrap(renderer, width, width_mode, height, height_mode);
  }
}

static bool sDisplayNoneFlag = false;

void CSSStaticLayout::Layout(LayoutObject* renderer, int width, int height) {
  const CSSStyle* item_style = &(renderer->css_style());
  // 如果是隐藏的view，之后的view都不进行排版，需要重新measure
  if (sDisplayNoneFlag) {
    LayoutWhenDisplayNone(renderer);
  } else if (item_style->css_display_type_ != CSS_DISPLAY_FLEX) {
    sDisplayNoneFlag = true;
    LayoutWhenDisplayNone(renderer);
    sDisplayNoneFlag = false;
  } else if (item_style->flex_direction_ == CSSFLEX_DIRECTION_ROW ||
             item_style->flex_direction_ == CSSFLEX_DIRECTION_ROW_REVERSE) {
    LayoutRow(renderer, width, height);
  } else if (item_style->flex_direction_ == CSSFLEX_DIRECTION_COLUMN ||
             item_style->flex_direction_ == CSSFLEX_DIRECTION_COLUMN_REVERSE) {
    LayoutColumn(renderer, width, height);
  }
}

void CSSStaticLayout::LayoutWhenDisplayNone(LayoutObject* renderer) {
  for (int i = 0; i < renderer->GetChildCount(); i++) {
    LayoutObject* child = (LayoutObject*)renderer->Find(i);
    child->Layout(0, 0, 0, 0);
  }
}

// 分类处理row的几种情况
void CSSStaticLayout::LayoutRow(LayoutObject* renderer, int width, int height) {
  const CSSStyle* item_style = &(renderer->css_style());
  if (item_style->flex_wrap_ == CSSFLEX_NOWRAP) {
    LayoutRowOneLine(renderer, width, height);
  } else if (item_style->flex_wrap_ == CSSFLEX_WRAP ||
             item_style->flex_wrap_ == CSSFLEX_WRAP_REVERSE) {
    LayoutRowWrap(renderer, width, height);
  }
}

// flex-wrap:wrap & wrap-reverse; flex-direction:row & row-reverse
void CSSStaticLayout::LayoutRowWrap(LayoutObject* renderer,
                                    int width,
                                    int height) {
  const CSSStyle* item_style = &(renderer->css_style());
  // 可用宽高为去除padding和border之后的宽高
  int available_width = width - item_style->padding_left_ -
                        item_style->padding_right_ -
                        item_style->border_width_ * 2;
  int available_height = height - item_style->padding_top_ -
                         item_style->padding_bottom_ -
                         item_style->border_width_ * 2;

  int current_row_without_absolute_count = 0;
  int total_use_width_without_absolute = 0;

  // rflag用来判断是否是row-reverse
  bool rflag = item_style->flex_direction_ == CSSFLEX_DIRECTION_ROW_REVERSE
                   ? true
                   : false;
  // wrflag用来判断是否是wrap-reverse
  bool wrflag = item_style->flex_wrap_ == CSSFLEX_WRAP_REVERSE ? true : false;

  int start = 0;
  int total_used_height = 0;

  int current_line_height = 0;
  bool is_line_feed = false;

  // child_list中所有flex-item
  vector<LayoutObject*> child_list;
  child_list.reserve(renderer->GetChildCount());
  for (int index = 0, child_view_count = renderer->GetChildCount();
       index < child_view_count; index++) {
    LayoutObject* child = (LayoutObject*)renderer->Find(index);
    const CSSStyle* child_style = &(child->css_style());

    if (child_style->css_display_type_ != CSS_DISPLAY_FLEX) {
      child->Layout(0, 0, 0, 0);
      continue;
    }
    if (child_style->css_position_type_ == CSS_POSITION_ABSOLUTE) {
      LayoutAbsolute(renderer, child, width, height);
      continue;
    }
    if (child_style->css_position_type_ == CSS_POSITION_FIXED) {
      LayoutFixed(renderer, child);
      continue;
    }
    child_list.push_back(child);
    current_row_without_absolute_count++;
  }
  child_list.resize(current_row_without_absolute_count);
  current_row_without_absolute_count = 0;

  // order排序
  sort(child_list.begin(), child_list.end(), CompareFlexOrder);

  for (int index = 0, child_view_count = child_list.size();
       index < child_view_count; index++) {
    LayoutObject* child = child_list[index];
    const CSSStyle* child_style = &(child->css_style());

    int oldTotalUseWidthWithoutAbsolute = total_use_width_without_absolute;

    current_row_without_absolute_count++;

    total_use_width_without_absolute += child->measured_size_.width_ +
                                        child_style->margin_left_ +
                                        child_style->margin_right_;

    if (total_use_width_without_absolute <= available_width) {
      int childHeight = child->measured_size_.height_ +
                        child_style->margin_top_ + child_style->margin_bottom_;
      current_line_height =
          current_line_height > childHeight ? current_line_height : childHeight;
    }

    // 遍历到最后一个孩子的时候，不可以跳过layout previous
    // child的操作，否则之前的孩子不能排版
    bool is_last_view = (index == child_view_count - 1);

    if (total_use_width_without_absolute > available_width || is_last_view) {
      if (total_use_width_without_absolute > available_width &&
          current_row_without_absolute_count > 1) {
        is_line_feed = true;
        index--;
        current_row_without_absolute_count--;
        total_use_width_without_absolute = oldTotalUseWidthWithoutAbsolute;
      } else if (is_last_view && !is_line_feed) {
        current_line_height = available_height;
      }

      int adjust_width_start = 0;
      int adjust_width_interval = 0;
      if (current_row_without_absolute_count > 0) {
        if (item_style->flex_justify_content_ == CSSFLEX_JUSTIFY_FLEX_START &&
            rflag) {
          adjust_width_start =
              available_width - total_use_width_without_absolute;
        } else if (item_style->flex_justify_content_ ==
                       CSSFLEX_JUSTIFY_FLEX_END &&
                   !rflag) {
          adjust_width_start =
              available_width - total_use_width_without_absolute;
        } else if (item_style->flex_justify_content_ ==
                   CSSFLEX_JUSTIFY_FLEX_CENTER) {
          adjust_width_start = round(
              (float)(available_width - total_use_width_without_absolute) /
              2.0f);
        } else if (total_use_width_without_absolute <= available_width &&
                   item_style->flex_justify_content_ ==
                       CSSFLEX_JUSTIFY_SPACE_BETWEEN) {
          if (current_row_without_absolute_count > 1) {
            adjust_width_interval = round(
                ((float)(available_width - total_use_width_without_absolute)) /
                (current_row_without_absolute_count - 1));
          }
        } else if (total_use_width_without_absolute <= available_width &&
                   item_style->flex_justify_content_ ==
                       CSSFLEX_JUSTIFY_SPACE_AROUND) {
          float interval =
              ((float)(available_width - total_use_width_without_absolute)) /
              (current_row_without_absolute_count * 2);
          adjust_width_start = round(interval);
          adjust_width_interval = round(interval * 2);
        }
      }

      int max_height = 0;
      int child_origin_x = item_style->padding_left_ +
                           item_style->border_width_ + adjust_width_start;
      // origin_y:wrap时为一行元素的top,padding-top + border-top + 已用行高
      // wrap-reverse时为一行元素的bottom,即
      // padding-top + border-top + available-height - 已用行高
      int child_origin_y =
          item_style->padding_top_ + item_style->border_width_ +
          (!wrflag ? total_used_height : available_height - total_used_height);

      // reverse时翻转当前行元素
      if (rflag)
        reverse(child_list.begin() + start, child_list.begin() + index + 1);
      for (int i = start; i <= index; i++) {
        LayoutObject* recalc_child = child_list[i];
        const CSSStyle* recalc_child_style = &(recalc_child->css_style());

        int align = item_style->flex_align_items_;

        if (recalc_child_style->flex_align_self_ != CSSFLEX_ALIGN_AUTO) {
          align = recalc_child_style->flex_align_self_;
        }

        int old_child_origin_y = child_origin_y;

        child_origin_x += recalc_child_style->margin_left_;
        // wrap时加元素的margin-top, wrap-reverse时减元素的margin-bottom
        child_origin_y += (!wrflag ? recalc_child_style->margin_top_
                                   : -recalc_child_style->margin_bottom_);
        if (i > start)
          child_origin_x += adjust_width_interval;

        int adjust_y = 0;
        int adjust_height = CSS_UNDEFINED;
        if (align == CSSFLEX_ALIGN_FLEX_START ||
            align == CSSFLEX_ALIGN_FLEX_START) {
          // do nothing
        } else if (align == CSSFLEX_ALIGN_FLEX_END ||
                   align == CSSFLEX_ALIGN_FLEX_END) {
          adjust_y = current_line_height - recalc_child_style->margin_bottom_ -
                     recalc_child->measured_size_.height_;
        } else if (align == CSSFLEX_ALIGN_STRETCH ||
                   align == CSSFLEX_ALIGN_STRETCH) {
          adjust_height = current_line_height -
                          recalc_child_style->margin_top_ -
                          recalc_child_style->margin_bottom_;
          adjust_height = recalc_child->css_style().ClampHeight(adjust_height);
        } else if (align == CSSFLEX_ALIGN_CENTER ||
                   align == CSSFLEX_ALIGN_CENTER) {
          adjust_y = round((float)(current_line_height -
                                   recalc_child->measured_size_.height_) /
                           2.0f);
        }

        int l = child_origin_x;
        int r = l + recalc_child->measured_size_.width_;
        // wrap时，先计算t，再计算b
        // wrap-reverse时，先计算b，再计算t
        int t = 0, b = 0;
        if (!wrflag) {
          t = child_origin_y + adjust_y;
          b = CSS_IS_UNDEFINED(adjust_height)
                  ? t + recalc_child->measured_size_.height_
                  : t + adjust_height;
        } else {
          b = child_origin_y - adjust_y;
          t = CSS_IS_UNDEFINED(adjust_height)
                  ? b - recalc_child->measured_size_.height_
                  : b - adjust_height;
        }

        recalc_child->Layout(l, t, r, b);

        child_origin_x += recalc_child->measured_size_.width_ +
                          recalc_child_style->margin_right_;
        child_origin_y = old_child_origin_y;
        int child_item_height = (b - t) + recalc_child_style->margin_bottom_ +
                                recalc_child_style->margin_top_;
        max_height =
            max_height < child_item_height ? child_item_height : max_height;
      }
      start = index + 1;
      total_use_width_without_absolute =
          item_style->padding_left_ + item_style->padding_right_;
      current_row_without_absolute_count = 0;
      total_used_height += max_height;
      current_line_height = 0;
    }
  }
}

// flex-wrap: nowrap; flex-direction: row
void CSSStaticLayout::LayoutRowOneLine(LayoutObject* renderer,
                                       int width,
                                       int height) {
  const CSSStyle* item_style = &(renderer->css_style());
  // 可用宽高为去除padding和border之后的宽高
  int available_width = width - item_style->padding_left_ -
                        item_style->padding_right_ -
                        item_style->border_width_ * 2;
  int available_height = height - item_style->padding_top_ -
                         item_style->padding_bottom_ -
                         item_style->border_width_ * 2;

  int current_row_without_absolute_count = 0;
  int total_use_width_without_absolute = 0;
  // child_list中所有flex-item
  vector<LayoutObject*> child_list;
  child_list.reserve(renderer->GetChildCount());

  // 计算所有flex-item的总width
  for (int index = 0, child_view_count = renderer->GetChildCount();
       index < child_view_count; index++) {
    LayoutObject* child = (LayoutObject*)renderer->Find(index);
    const CSSStyle* child_style = &(child->css_style());

    if (child_style->css_display_type_ != CSS_DISPLAY_FLEX) {
      child->Layout(0, 0, 0, 0);
      continue;
    }
    if (child_style->css_position_type_ == CSS_POSITION_ABSOLUTE) {
      LayoutAbsolute(renderer, child, width, height);
      continue;
    }
    if (child_style->css_position_type_ == CSS_POSITION_FIXED) {
      LayoutFixed(renderer, child);
      continue;
    }
    child_list.push_back(child);
    current_row_without_absolute_count++;
    total_use_width_without_absolute += child->measured_size_.width_ +
                                        child->css_style().margin_left_ +
                                        child->css_style().margin_right_;
  }
  child_list.resize(current_row_without_absolute_count);
  // rflag
  bool rflag = item_style->flex_direction_ == CSSFLEX_DIRECTION_ROW_REVERSE
                   ? true
                   : false;

  // order排序，reverse翻转
  sort(child_list.begin(), child_list.end(), CompareFlexOrder);
  if (rflag)
    reverse(child_list.begin(), child_list.end());

  int adjust_width_start = 0;
  int adjust_width_interval = 0;
  if (current_row_without_absolute_count > 0) {
    if (item_style->flex_justify_content_ == CSSFLEX_JUSTIFY_FLEX_START &&
        rflag) {
      adjust_width_start = available_width - total_use_width_without_absolute;
    } else if (item_style->flex_justify_content_ == CSSFLEX_JUSTIFY_FLEX_END &&
               !rflag) {
      adjust_width_start = available_width - total_use_width_without_absolute;
    } else if (item_style->flex_justify_content_ ==
               CSSFLEX_JUSTIFY_FLEX_CENTER) {
      adjust_width_start = round(
          (float)(available_width - total_use_width_without_absolute) / 2.0f);
    } else if (total_use_width_without_absolute <= available_width &&
               item_style->flex_justify_content_ ==
                   CSSFLEX_JUSTIFY_SPACE_BETWEEN) {
      if (current_row_without_absolute_count > 1) {
        adjust_width_interval = round(
            ((float)(available_width - total_use_width_without_absolute)) /
            (current_row_without_absolute_count - 1));
      }
    } else if (total_use_width_without_absolute <= available_width &&
               item_style->flex_justify_content_ ==
                   CSSFLEX_JUSTIFY_SPACE_AROUND) {
      float interval =
          ((float)(available_width - total_use_width_without_absolute)) /
          (current_row_without_absolute_count * 2);
      adjust_width_start = round(interval);
      adjust_width_interval = round(interval * 2);
    }
  }

  int child_origin_x = adjust_width_start + item_style->padding_left_ +
                       item_style->border_width_;
  int child_origin_y = item_style->padding_top_ + item_style->border_width_;

  for (LayoutObject* child : child_list) {
    const CSSStyle* child_style = &(child->css_style());

    int align = item_style->flex_align_items_;

    if (child_style->flex_align_self_ != CSSFLEX_ALIGN_AUTO) {
      align = child_style->flex_align_self_;
    }

    int old_child_origin_y = child_origin_y;

    child_origin_x += child_style->margin_left_;
    if (child != child_list[0])
      child_origin_x += adjust_width_interval;
    child_origin_y += child_style->margin_top_;

    int adjust_y = 0;
    int adjust_height = CSS_UNDEFINED;
    if (align == CSSFLEX_ALIGN_FLEX_START) {
      // do nothing
    } else if (align == CSSFLEX_ALIGN_FLEX_END) {
      adjust_y = available_height - child_style->margin_bottom_ -
                 child->measured_size_.height_;
    } else if (align == CSSFLEX_ALIGN_STRETCH) {
      if (CSS_IS_UNDEFINED(child_style->height_)) {
        adjust_height = available_height - child_style->margin_top_ -
                        child_style->margin_bottom_;
        adjust_height = child->css_style().ClampHeight(adjust_height);
      }
    } else if (align == CSSFLEX_ALIGN_CENTER) {
      adjust_y = round(
          (float)(available_height - child->measured_size_.height_) / 2.0f);
    }

    int l = child_origin_x;
    int t = adjust_y + child_origin_y;
    int r = child_origin_x + child->measured_size_.width_;
    int b = CSS_IS_UNDEFINED(adjust_height)
                ? child_origin_y + child->measured_size_.height_ + adjust_y
                : child_origin_y + adjust_height;
    child->Layout(l, t, r, b);

    child_origin_x += child->measured_size_.width_ + child_style->margin_right_;
    child_origin_y = old_child_origin_y;
  }
}

void CSSStaticLayout::LayoutColumn(LayoutObject* renderer,
                                   int width,
                                   int height) {
  const CSSStyle* item_style = &(renderer->css_style());
  if (item_style->flex_wrap_ == CSSFLEX_NOWRAP) {
    LayoutColumnOneLine(renderer, width, height);
  } else if (item_style->flex_wrap_ == CSSFLEX_WRAP ||
             item_style->flex_wrap_ == CSSFLEX_WRAP_REVERSE) {
    LayoutColumnWrap(renderer, width, height);
  }
}

// flex-wrap:wrap; flex-direction:column
void CSSStaticLayout::LayoutColumnWrap(LayoutObject* renderer,
                                       int width,
                                       int height) {
  const CSSStyle* item_style = &(renderer->css_style());

  int available_width = width - item_style->padding_left_ -
                        item_style->padding_right_ -
                        item_style->border_width_ * 2;
  int available_height = height - item_style->padding_top_ -
                         item_style->padding_bottom_ -
                         item_style->border_width_ * 2;

  int current_column_without_absolute_count = 0;
  int total_use_height_without_absolute = 0;

  // rflag判断是否为reverse
  bool rflag = item_style->flex_direction_ == CSSFLEX_DIRECTION_COLUMN_REVERSE
                   ? true
                   : false;
  // wrflag用来判断是否是wrap-reverse
  bool wrflag = item_style->flex_wrap_ == CSSFLEX_WRAP_REVERSE ? true : false;

  int start = 0;
  int total_used_width = 0;

  int current_line_width = 0;
  bool is_line_feed = false;

  // child_list中所有flex-item
  vector<LayoutObject*> child_list;
  child_list.reserve(renderer->GetChildCount());
  for (int index = 0, child_view_count = renderer->GetChildCount();
       index < child_view_count; index++) {
    LayoutObject* child = (LayoutObject*)renderer->Find(index);
    const CSSStyle* child_style = &(child->css_style());

    if (child_style->css_display_type_ != CSS_DISPLAY_FLEX) {
      child->Layout(0, 0, 0, 0);
      continue;
    }
    if (child_style->css_position_type_ == CSS_POSITION_ABSOLUTE) {
      LayoutAbsolute(renderer, child, width, height);
      continue;
    }
    if (child_style->css_position_type_ == CSS_POSITION_FIXED) {
      LayoutFixed(renderer, child);
      continue;
    }
    child_list.push_back(child);
    current_column_without_absolute_count++;
  }
  child_list.resize(current_column_without_absolute_count);
  current_column_without_absolute_count = 0;

  // order排序
  sort(child_list.begin(), child_list.end(), CompareFlexOrder);

  for (int index = 0, child_view_count = child_list.size();
       index < child_view_count; index++) {
    LayoutObject* child = child_list[index];
    const CSSStyle* child_style = &(child->css_style());

    int oldTotalUseHeightWithoutAbsolute = total_use_height_without_absolute;
    current_column_without_absolute_count++;

    if (!CSS_IS_UNDEFINED(child_style->height_)) {
      total_use_height_without_absolute += child_style->height_ +
                                           child_style->margin_top_ +
                                           child_style->margin_bottom_;
    } else {
      total_use_height_without_absolute += child->measured_size_.height_ +
                                           child_style->margin_top_ +
                                           child_style->margin_bottom_;
    }

    if (total_use_height_without_absolute <= available_height) {
      int childWidth = child->measured_size_.width_ +
                       child_style->margin_left_ + child_style->margin_right_;
      current_line_width =
          current_line_width > childWidth ? current_line_width : childWidth;
    }

    // 遍历到最后一个孩子的时候，不可以跳过layout previous
    // child的操作，否则之前的孩子不能排版
    bool is_last_view = (index == child_view_count - 1);

    if (total_use_height_without_absolute > available_height || is_last_view) {
      if (total_use_height_without_absolute > available_height &&
          current_column_without_absolute_count > 1) {
        is_line_feed = true;
        index--;
        current_column_without_absolute_count--;
        total_use_height_without_absolute = oldTotalUseHeightWithoutAbsolute;
      } else if (is_last_view && !is_line_feed) {
        current_line_width = available_width;
      }

      int adjust_height_start = 0;
      int adjust_height_interval = 0;
      if (current_column_without_absolute_count > 0) {
        if (item_style->flex_justify_content_ == CSSFLEX_JUSTIFY_FLEX_START &&
            rflag) {
          adjust_height_start =
              available_height - total_use_height_without_absolute;
        } else if (item_style->flex_justify_content_ ==
                       CSSFLEX_JUSTIFY_FLEX_END &&
                   !rflag) {
          adjust_height_start =
              available_height - total_use_height_without_absolute;
        } else if (item_style->flex_justify_content_ ==
                   CSSFLEX_JUSTIFY_FLEX_CENTER) {
          adjust_height_start = round(
              (float)(available_height - total_use_height_without_absolute) /
              2.0f);
        } else if (total_use_height_without_absolute <= available_height &&
                   item_style->flex_justify_content_ ==
                       CSSFLEX_JUSTIFY_SPACE_BETWEEN) {
          if (current_column_without_absolute_count > 1) {
            adjust_height_interval =
                round(((float)(available_height -
                               total_use_height_without_absolute)) /
                      (current_column_without_absolute_count - 1));
          }
        } else if (total_use_height_without_absolute <= available_height &&
                   item_style->flex_justify_content_ ==
                       CSSFLEX_JUSTIFY_SPACE_AROUND) {
          float interval =
              ((float)(available_height - total_use_height_without_absolute)) /
              (current_column_without_absolute_count * 2);
          adjust_height_start = round(interval);
          adjust_height_interval = round(interval * 2);
        }
      }

      int maxWidth = 0;
      // origin_x:wrap时为一列元素的left,padding-left + border-left + 已用列宽
      // wrap-reverse时为一列元素的right,即
      // padding-left + border-left + available-width - 已用列宽
      int child_origin_x =
          item_style->padding_left_ + item_style->border_width_ +
          (!wrflag ? total_used_width : available_width - total_used_width);
      int child_origin_y = item_style->padding_top_ +
                           item_style->border_width_ + adjust_height_start;

      // reverse时翻转当前列元素
      if (rflag)
        reverse(child_list.begin() + start, child_list.begin() + index + 1);
      for (int i = start; i <= index; i++) {
        LayoutObject* recalc_child = child_list[i];
        const CSSStyle* recalc_child_style = &(recalc_child->css_style());

        int align = item_style->flex_align_items_;

        if (recalc_child_style->flex_align_self_ != CSSFLEX_ALIGN_AUTO) {
          align = recalc_child_style->flex_align_self_;
        }

        int old_child_origin_x = child_origin_x;
        // wrap时加元素的margin-left, wrap-reverse时减元素的margin-right
        child_origin_x += (!wrflag ? recalc_child_style->margin_left_
                                   : -recalc_child_style->margin_right_);
        child_origin_y += recalc_child_style->margin_top_;
        if (i > start)
          child_origin_y += adjust_height_interval;

        int adjust_x = 0;
        int adjust_width = CSS_UNDEFINED;
        if (align == CSSFLEX_ALIGN_FLEX_START) {
          // do nothing
        } else if (align == CSSFLEX_ALIGN_FLEX_END) {
          adjust_x = current_line_width - recalc_child_style->margin_right_ -
                     recalc_child->measured_size_.width_;
        } else if (align == CSSFLEX_ALIGN_STRETCH) {
          adjust_width = current_line_width -
                         recalc_child_style->margin_right_ -
                         recalc_child_style->margin_left_;
          adjust_width = recalc_child->css_style().ClampWidth(adjust_width);
        } else if (align == CSSFLEX_ALIGN_CENTER) {
          adjust_x = round((float)(current_line_width -
                                   recalc_child_style->margin_right_ -
                                   recalc_child_style->margin_left_ -
                                   recalc_child->measured_size_.width_) /
                           2.0f);
        }

        int l = 0, r = 0;
        int t = child_origin_y;
        int b = t + recalc_child->measured_size_.height_;
        if (!wrflag) {
          l = child_origin_x + adjust_x;
          r = CSS_IS_UNDEFINED(adjust_width)
                  ? l + recalc_child->measured_size_.width_
                  : l + adjust_width;
        } else {
          r = child_origin_x - adjust_x;
          l = CSS_IS_UNDEFINED(adjust_width)
                  ? r - recalc_child->measured_size_.width_
                  : r - adjust_width;
        }

        recalc_child->Layout(l, t, r, b);

        child_origin_x = old_child_origin_x;
        child_origin_y += recalc_child->measured_size_.height_ +
                          recalc_child_style->margin_bottom_;
        int child_item_width = (r - l) + recalc_child_style->margin_left_ +
                               recalc_child_style->margin_right_;
        maxWidth = maxWidth < child_item_width ? child_item_width : maxWidth;
      }
      start = index + 1;
      total_use_height_without_absolute =
          item_style->padding_top_ + item_style->padding_bottom_;
      current_column_without_absolute_count = 0;
      total_used_width += maxWidth;
      current_line_width = 0;
    }
  }
}

// flex-wrap: nowrap; flex-direction: column
void CSSStaticLayout::LayoutColumnOneLine(LayoutObject* renderer,
                                          int width,
                                          int height) {
  const CSSStyle* item_style = &(renderer->css_style());

  int available_width = width - item_style->padding_left_ -
                        item_style->padding_right_ -
                        item_style->border_width_ * 2;
  int available_height = height - item_style->padding_top_ -
                         item_style->padding_bottom_ -
                         item_style->border_width_ * 2;

  int current_column_without_absolute_count = 0;
  int total_use_height_without_absolute = 0;
  // child_list中所有flex-item
  vector<LayoutObject*> child_list;
  child_list.reserve(renderer->GetChildCount());

  // 计算所有flex-item的总width
  for (int index = 0, child_view_count = renderer->GetChildCount();
       index < child_view_count; index++) {
    LayoutObject* child = (LayoutObject*)renderer->Find(index);
    const CSSStyle* child_style = &(child->css_style());
    if (child_style->css_display_type_ != CSS_DISPLAY_FLEX) {
      child->Layout(0, 0, 0, 0);
      continue;
    }
    if (child_style->css_position_type_ == CSS_POSITION_ABSOLUTE) {
      LayoutAbsolute(renderer, child, width, height);
      continue;
    }
    if (child_style->css_position_type_ == CSS_POSITION_FIXED) {
      LayoutFixed(renderer, child);
      continue;
    }
    child_list.push_back(child);
    current_column_without_absolute_count++;
    total_use_height_without_absolute += child->measured_size_.height_ +
                                         child->css_style().margin_top_ +
                                         child->css_style().margin_bottom_;
  }
  child_list.resize(current_column_without_absolute_count);
  // rflag
  bool rflag = item_style->flex_direction_ == CSSFLEX_DIRECTION_COLUMN_REVERSE
                   ? true
                   : false;

  // order排序，reverse翻转
  sort(child_list.begin(), child_list.end(), CompareFlexOrder);
  if (rflag)
    reverse(child_list.begin(), child_list.end());

  int adjust_height_start = 0;
  int adjust_height_interval = 0;
  if (current_column_without_absolute_count > 0) {
    if (item_style->flex_justify_content_ == CSSFLEX_JUSTIFY_FLEX_START &&
        rflag) {
      adjust_height_start =
          available_height - total_use_height_without_absolute;
    } else if (item_style->flex_justify_content_ == CSSFLEX_JUSTIFY_FLEX_END &&
               !rflag) {
      adjust_height_start =
          available_height - total_use_height_without_absolute;
    } else if (item_style->flex_justify_content_ ==
               CSSFLEX_JUSTIFY_FLEX_CENTER) {
      adjust_height_start = round(
          (float)(available_height - total_use_height_without_absolute) / 2.0f);
    } else if (total_use_height_without_absolute <= available_height &&
               item_style->flex_justify_content_ ==
                   CSSFLEX_JUSTIFY_SPACE_BETWEEN) {
      if (current_column_without_absolute_count > 1) {
        adjust_height_interval = round(
            ((float)(available_height - total_use_height_without_absolute)) /
            (current_column_without_absolute_count - 1));
      }
    } else if (total_use_height_without_absolute <= available_height &&
               item_style->flex_justify_content_ ==
                   CSSFLEX_JUSTIFY_SPACE_AROUND) {
      float interval =
          ((float)(available_height - total_use_height_without_absolute)) /
          (current_column_without_absolute_count * 2);
      adjust_height_start = round(interval);
      adjust_height_interval = round(interval * 2);
    }
  }

  int child_origin_x = item_style->padding_left_ + item_style->border_width_;
  int child_origin_y = item_style->padding_top_ + item_style->border_width_ +
                       adjust_height_start;
  for (LayoutObject* child : child_list) {
    const CSSStyle* child_style = &(child->css_style());

    int align = item_style->flex_align_items_;

    if (child_style->flex_align_self_ != CSSFLEX_ALIGN_AUTO) {
      align = child_style->flex_align_self_;
    }
    int old_child_origin_x = child_origin_x;

    child_origin_x += child_style->margin_left_;
    child_origin_y += child_style->margin_top_;
    if (child != child_list[0])
      child_origin_y += adjust_height_interval;

    int adjust_x = 0;
    int adjust_width = CSS_UNDEFINED;
    if (align == CSSFLEX_ALIGN_FLEX_START) {
      // do nothing
    } else if (align == CSSFLEX_ALIGN_FLEX_END) {
      adjust_x = available_width - child_style->margin_right_ -
                 child->measured_size_.width_;
    } else if (align == CSSFLEX_ALIGN_STRETCH) {
      if (CSS_IS_UNDEFINED(child_style->width_)) {
        adjust_width = available_width - child_style->margin_right_ -
                       child_style->margin_left_;
        adjust_width = child->css_style().ClampWidth(adjust_width);
      }
    } else if (align == CSSFLEX_ALIGN_CENTER) {
      adjust_x = (available_width - child_style->margin_right_ -
                  child_style->margin_left_ - child->measured_size_.width_) /
                 2;
    }

    int l = adjust_x + child_origin_x;
    int t = child_origin_y;
    int r = CSS_IS_UNDEFINED(adjust_width)
                ? child_origin_x + child->measured_size_.width_ + adjust_x
                : adjust_width + child_origin_x;
    int b = child_origin_y + child->measured_size_.height_;

    child->Layout(l, t, r, b);

    child_origin_x = old_child_origin_x;
    child_origin_y +=
        child->measured_size_.height_ + child_style->margin_bottom_;
  }
}

void CSSStaticLayout::MeasureAbsolute(LayoutObject* renderer,
                                      int width,
                                      int width_mode,
                                      int height,
                                      int height_mode) {
  int w = CSS_IS_UNDEFINED(renderer->css_style().width_)
              ? width
              : renderer->css_style().width_;
  int h = CSS_IS_UNDEFINED(renderer->css_style().height_)
              ? height
              : renderer->css_style().height_;

  w -= CSS_IS_UNDEFINED(renderer->css_style().right_)
           ? 0
           : renderer->css_style().right_;
  w -= CSS_IS_UNDEFINED(renderer->css_style().left_)
           ? 0
           : renderer->css_style().left_;
  h -= CSS_IS_UNDEFINED(renderer->css_style().top_)
           ? 0
           : renderer->css_style().top_;
  h -= CSS_IS_UNDEFINED(renderer->css_style().bottom_)
           ? 0
           : renderer->css_style().bottom_;

  w -= renderer->css_style().margin_left_ + renderer->css_style().margin_right_;
  h -= renderer->css_style().margin_top_ + renderer->css_style().margin_bottom_;

  renderer->Measure(
      base::Size::Descriptor::Make(w, base::Size::Descriptor::AT_MOST),
      base::Size::Descriptor::Make(h, base::Size::Descriptor::AT_MOST));
}

void CSSStaticLayout::MeasureFixed(LayoutObject* renderer) {
#ifndef TESTING
  int width = static_cast<RenderObject*>(renderer)
                  ->render_tree_host()
                  ->viewport()
                  .GetWidth();
  int height = static_cast<RenderObject*>(renderer)
                   ->render_tree_host()
                   ->viewport()
                   .GetHeight();
  const CSSStyle* parent_style = &(static_cast<RenderObject*>(renderer)
                                       ->render_tree_host()
                                       ->render_root()
                                       ->css_style());

  int available_width = width - parent_style->border_width_ * 2 -
                        parent_style->padding_right_ -
                        parent_style->padding_left_;
  int available_height = height - parent_style->border_width_ * 2 -
                         parent_style->padding_top_ -
                         parent_style->padding_bottom_;
#else
  int available_width = 0;
  int available_height = 0;
#endif
  // The same logic
  MeasureAbsolute(renderer, available_width, 0, available_height, 0);
}

void CSSStaticLayout::LayoutAbsolute(LayoutObject* parentNode,
                                     LayoutObject* renderer,
                                     int width,
                                     int height) {
  LayoutFixedOrAbsolute(parentNode, renderer, width, height);
}

void CSSStaticLayout::LayoutFixed(LayoutObject* parentNode,
                                  LayoutObject* renderer) {
#ifndef TESTING
  int width = static_cast<RenderObject*>(renderer)
                  ->render_tree_host()
                  ->viewport()
                  .GetWidth();
  int height = static_cast<RenderObject*>(renderer)
                   ->render_tree_host()
                   ->viewport()
                   .GetHeight();
#else
  int width = 0;
  int height = 0;
#endif
  LayoutFixedOrAbsolute(parentNode, renderer, width, height);
}

void CSSStaticLayout::LayoutFixedOrAbsolute(LayoutObject* parent,
                                            LayoutObject* child,
                                            int width,
                                            int height) {
  const CSSStyle* parent_style = &(parent->css_style());
  const CSSStyle* child_style = &(child->css_style());

  int l = 0, r = 0, t = 0, b = 0;

  int offset_left = child_style->margin_left_ + parent_style->border_width_;
  int offset_right = child_style->margin_right_ + parent_style->border_width_;
  int offset_top = child_style->margin_top_ + parent_style->border_width_;
  int offset_bottom = child_style->margin_bottom_ + parent_style->border_width_;

  if (CSS_IS_UNDEFINED(child_style->left_) &&
      CSS_IS_UNDEFINED(child_style->right_)) {
    int adjust = CalculateOffsetWithFlexContainerStyle(parent, child, width,
                                                       CSS_UNDEFINED);

    l = adjust + offset_left + parent_style->padding_left_;
    r = l + child->measured_size_.width_;
  } else if (!CSS_IS_UNDEFINED(child_style->left_) &&
             !CSS_IS_UNDEFINED(child_style->right_)) {
    if (CSS_IS_UNDEFINED(child_style->width_)) {
      l = child_style->left_ + offset_left;
      r = width - child_style->right_ - offset_right;
      if (l > r) {
        r = l;
      }
    } else {
      l = child_style->left_ + offset_left;
      r = l + child->measured_size_.width_;
    }
  } else if (!CSS_IS_UNDEFINED(child_style->left_)) {
    l = child_style->left_ + offset_left;
    r = l + child->measured_size_.width_;
  } else if (!CSS_IS_UNDEFINED(child_style->right_)) {
    r = width - child_style->right_ - offset_right;
    l = r - child->measured_size_.width_;
  } else {
    l = l + offset_left;
    r = l + child->measured_size_.width_;
  }

  if (CSS_IS_UNDEFINED(child_style->top_) &&
      CSS_IS_UNDEFINED(child_style->bottom_)) {
    int adjust = CalculateOffsetWithFlexContainerStyle(parent, child,
                                                       CSS_UNDEFINED, height);

    t = adjust + offset_top + parent_style->padding_top_;
    b = t + child->measured_size_.height_;
  } else if (!CSS_IS_UNDEFINED(child_style->top_) &&
             !CSS_IS_UNDEFINED(child_style->bottom_)) {
    if (CSS_IS_UNDEFINED(child_style->height_)) {
      t = child_style->top_ + offset_top;
      b = height - child_style->bottom_ - offset_bottom;
      if (t > b) {
        b = t;
      }
    } else {
      t = child_style->top_ + offset_top;
      b = t + child->measured_size_.height_;
    }
  } else if (!CSS_IS_UNDEFINED(child_style->top_)) {
    t = child_style->top_ + offset_top;
    b = t + child->measured_size_.height_;
  } else if (!CSS_IS_UNDEFINED(child_style->bottom_)) {
    b = height - child_style->bottom_ - offset_bottom;
    t = b - child->measured_size_.height_;
  } else {
    t = t + offset_top;
    b = t + child->measured_size_.height_;
  }

  child->Layout(l, t, r, b);
}

int CSSStaticLayout::CalculateOffsetWithFlexContainerStyle(LayoutObject* parent,
                                                           LayoutObject* child,
                                                           int width,
                                                           int height) {
  int offset = 0;
  bool is_width_available = !CSS_IS_UNDEFINED(width);

  const CSSStyle* parent_style = &(parent->css_style());
  const CSSStyle* child_style = &(child->css_style());

  int available_width = CSS_UNDEFINED;
  int available_height = CSS_UNDEFINED;

  if (is_width_available) {
    available_width = width - parent_style->padding_left_ -
                      parent_style->padding_right_ -
                      parent_style->border_width_ * 2;
  } else {
    available_height = height - parent_style->padding_top_ -
                       parent_style->padding_bottom_ -
                       parent_style->border_width_ * 2;
  }

  bool isMainAxis = (is_width_available &&
                     parent_style->flex_direction_ == CSSFLEX_DIRECTION_ROW) ||
                    (!is_width_available &&
                     parent_style->flex_direction_ == CSSFLEX_DIRECTION_COLUMN);

  int available_target_axis, child_size_on_target_axis;

  if (is_width_available) {
    available_target_axis = available_width;
    child_size_on_target_axis = child->measured_size_.width_;
  } else {
    available_target_axis = available_height;
    child_size_on_target_axis = child->measured_size_.height_;
  }

  if (isMainAxis) {
    // Main axis
    if (parent_style->flex_justify_content_ == CSSFLEX_JUSTIFY_FLEX_START ||
        parent_style->flex_justify_content_ == CSSFLEX_JUSTIFY_SPACE_BETWEEN) {
      // no action
    } else if (parent_style->flex_justify_content_ ==
               CSSFLEX_JUSTIFY_FLEX_END) {
      offset = available_target_axis - child_size_on_target_axis;
    } else if (parent_style->flex_justify_content_ ==
                   CSSFLEX_JUSTIFY_FLEX_CENTER ||
               parent_style->flex_justify_content_ ==
                   CSSFLEX_JUSTIFY_SPACE_AROUND) {
      offset = round(
          (float)(available_target_axis - child_size_on_target_axis) / 2.0f);
    }
  } else {
    // Cross axis
    int align = parent_style->flex_align_items_;

    if (child_style->flex_align_self_ != CSSFLEX_ALIGN_AUTO) {
      align = child_style->flex_align_self_;
    }

    if (parent_style->flex_align_items_ == CSSFLEX_ALIGN_FLEX_START) {
      // do nothing
    } else if (align == CSSFLEX_ALIGN_FLEX_END ||
               align == CSSFLEX_ALIGN_FLEX_END) {
      offset = available_target_axis - child_size_on_target_axis;
    } else if (align == CSSFLEX_ALIGN_STRETCH ||
               align == CSSFLEX_ALIGN_STRETCH) {
      // Nothing to do when position: absolute/fixed
    } else if (align == CSSFLEX_ALIGN_CENTER || align == CSSFLEX_ALIGN_CENTER) {
      offset = round(
          (float)(available_target_axis - child_size_on_target_axis) / 2.0f);
    }
  }

  return offset;
}

LayoutObject* CSSStaticLayout::GetRoot(LayoutObject* renderer) {
  ContainerNode* node = renderer;
  while (node->parent()) {
    node = node->parent();
  }
  return static_cast<LayoutObject*>(node);
}
}  // namespace lynx
