﻿#include "LongUI.h"
#include <algorithm>

// ------------------------- UIContainerBuiltIn ------------------------
// UIContainerBuiltIn: 事件处理
bool LongUI::UIContainerBuiltIn::DoEvent(const LongUI::EventArgument& arg) noexcept {
    // 处理窗口事件
    if (arg.sender) {
        switch (arg.event)
        {
        case LongUI::Event::Event_TreeBulidingFinished:
            // 初始化边缘控件 
            Super::DoEvent(arg);
            // 初次完成空间树建立
            for (auto ctrl : (*this)) {
                ctrl->DoEvent(arg);
            }
            return true;
        }
    }
    return Super::DoEvent(arg);
}

// UIContainerBuiltIn: 主景渲染
void LongUI::UIContainerBuiltIn::render_chain_main() const noexcept {
    // 渲染帮助器
    Super::RenderHelper(this->begin(), this->end());
    // 父类主景
    Super::render_chain_main();
}

// UIContainerBuiltIn: 渲染函数
void LongUI::UIContainerBuiltIn::Render() const noexcept {
    // 背景渲染
    this->render_chain_background();
    // 主景渲染
    this->render_chain_main();
    // 前景渲染
    this->render_chain_foreground();
}

// LongUI内建容器: 刷新
void LongUI::UIContainerBuiltIn::Update() noexcept {
    // 帮助器
    Super::UpdateHelper<Super>(this->begin(), this->end());
}


// UIContainerBuiltIn: 重建
auto LongUI::UIContainerBuiltIn::Recreate() noexcept -> HRESULT {
    HRESULT hr = S_OK;
    // 重建子控件
    for (auto ctrl : (*this)) {
        hr = ctrl->Recreate();
        assert(SUCCEEDED(hr));
    }
    return Super::Recreate();
}


/// <summary>
/// Find the control via mouse point
/// </summary>
/// <param name="pt">The wolrd mouse point.</param>
/// <returns>the control pointer, maybe nullptr</returns>
auto LongUI::UIContainerBuiltIn::FindChild(const D2D1_POINT_2F& pt) noexcept->UIControl* {
    // 父类(边缘控件)
    auto mctrl = Super::FindChild(pt);
    if (mctrl) return mctrl;
    // 性能警告
#ifdef _DEBUG
    if (this->GetCount() > 100) {
        UIManager << DL_Warning
            << "Performance Warning: O(n) algorithm"
            << " is not fine for container that over 100 children"
            << LongUI::endl;
    }
#endif
    for (auto ctrl : (*this)) {
        // 区域内判断
        if (IsPointInRect(ctrl->visible_rect, pt)) {
            return ctrl;
        }
    }
    return nullptr;
}


// UIContainerBuiltIn: 推入♂最后
void LongUI::UIContainerBuiltIn::PushBack(UIControl* child) noexcept {
    // 边界控件交给父类处理
    if (child && (child->flags & Flag_MarginalControl)) {
        Super::PushBack(child);
    }
    // 一般的就自己处理
    else {
        this->Insert(this->end(), child);
    }
}

// UIContainerBuiltIn: 仅插入控件
void LongUI::UIContainerBuiltIn::insert_only(Iterator itr, UIControl* ctrl) noexcept {
    const auto end_itr = this->end();
    assert(ctrl && "bad arguments");
    if (ctrl->prev) {
        UIManager << DL_Warning
            << L"the 'prev' attr of the control: ["
            << ctrl->name
            << "] that to insert is not null"
            << LongUI::endl;
    }
    if (ctrl->next) {
        UIManager << DL_Warning
            << L"the 'next' attr of the control: ["
            << ctrl->name
            << "] that to insert is not null"
            << LongUI::endl;
    }
    // 插入尾部?
    if (itr == end_itr) {
        // 链接
        force_cast(ctrl->prev) = m_pTail;
        // 无尾?
        if (m_pTail) force_cast(m_pTail->next) = ctrl;
        // 无头?
        if (!m_pHead) m_pHead = ctrl;
        // 设置尾
        m_pTail = ctrl;
    }
    else {
        // 一般般
        force_cast(ctrl->next) = itr.Ptr();
        force_cast(ctrl->prev) = itr->prev;
        if (itr->prev) {
            force_cast(itr->prev) = ctrl;
        }
        force_cast(itr->prev) = ctrl;
    }
    ++m_cChildrenCount;
}


// UIContainerBuiltIn: 仅移除控件
void LongUI::UIContainerBuiltIn::RemoveJust(UIControl* ctrl) noexcept {
    // 检查是否属于本容器
#ifdef _DEBUG
    bool ok = false;
    for (auto i : (*this)) {
        if (ctrl == i) {
            ok = true;
            break;
        }
    }
    if (!ok) {
        UIManager << DL_Error 
            << "control:[" 
            << ctrl->name
            << "] not in this container: " 
            << this->name 
            << LongUI::endl;
        return;
    }
#endif
    {
        // 连接前后节点
        register auto prev_tmp = ctrl->prev;
        register auto next_tmp = ctrl->next;
        // 检查, 头
        (prev_tmp ? force_cast(prev_tmp->next) : m_pHead) = next_tmp;
        // 检查, 尾
        (next_tmp ? force_cast(next_tmp->prev) : m_pTail) = prev_tmp;
        // 减少
        force_cast(ctrl->prev) = force_cast(ctrl->next) = nullptr;
        --m_cChildrenCount;
        // 修改
        this->SetControlLayoutChanged();
    }
    Super::RemoveJust(ctrl);
}


// UIContainerBuiltIn: 析构函数
LongUI::UIContainerBuiltIn::~UIContainerBuiltIn() noexcept {
    // 关闭子控件
    auto ctrl = m_pHead;
    while (ctrl) {
        auto next_ctrl = ctrl->next;
        this->cleanup_child(ctrl);
        ctrl = next_ctrl;
    }
}

// 获取控件索引
auto LongUI::UIContainerBuiltIn::GetIndexOf(UIControl* child) const noexcept ->uint32_t {
    assert(this == child->parent && "不是亲生的");
    uint32_t index = 0;
    for (auto ctrl : (*this)) {
        if (ctrl == child) break;
        ++index;
    }
    return index;
}

// 随机访问控件
auto LongUI::UIContainerBuiltIn::GetAt(uint32_t i) const noexcept -> UIControl * {
   // 超出
    if (i >= m_cChildrenCount) return nullptr;
    // 第一个
    if (!i) return m_pHead;
    // 性能警告
    if (i > 8) {
        UIManager << DL_Warning
            << L"Performance Warning! random accessig is not fine for list"
            << LongUI::endl;
    }
    // 检查范围
    if (i >= this->GetCount()) {
        UIManager << DL_Error << L"out of range" << LongUI::endl;
        return nullptr;
    }
    // 只有一个?
    if (this->GetCount() == 1) return m_pHead;
    // 前半部分?
    UIControl * control;
    if (i < this->GetCount() / 2) {
        control = m_pHead;
        while (i) {
            assert(control && "null pointer");
            control = control->next;
            --i;
        }
    }
    // 后半部分?
    else {
        control = m_pTail;
        i = static_cast<uint32_t>(this->GetCount()) - i - 1;
        while (i) {
            assert(control && "null pointer");
            control = control->prev;
            --i;
        }
    }
    return control;
}

// 交换
void LongUI::UIContainerBuiltIn::SwapChild(Iterator itr1, Iterator itr2) noexcept {
    auto ctrl1 = *itr1; auto ctrl2 = *itr2;
    assert(ctrl1 && ctrl2 && "bad arguments");
    assert(ctrl1->parent == this && ctrl2->parent == this && L"隔壁老王!");
    // 不一致时
    if (ctrl1 != ctrl2) {
        // A link B
        const bool a___b = ctrl1->next == ctrl2;
        // B link A
        const bool b___a = ctrl2->next == ctrl1;
        // A存在前驱
        if (ctrl1->prev) {
            if(!b___a) force_cast(ctrl1->prev->next) = ctrl2;
        }
        // A为头节点
        else {
            m_pHead = ctrl2;
        }
        // A存在后驱
        if (ctrl1->next) {
            if(!a___b) force_cast(ctrl1->next->prev) = ctrl2;
        }
        // A为尾节点
        else {
            m_pTail = ctrl2;
        }
        // B存在前驱
        if (ctrl2->prev) {
            if(!a___b) force_cast(ctrl2->prev->next) = ctrl1;
        }
        // B为头节点
        else {
            m_pHead = ctrl1;
        }
        // B存在后驱
        if (ctrl2->next) {
            if(!b___a) force_cast(ctrl2->next->prev) = ctrl1;
        }
        // B为尾节点
        else {
            m_pTail = ctrl1;
        }
        // 相邻交换
        auto swap_neibergs = [](UIControl* a, UIControl* b) noexcept {
            assert(a->next == b && "bad neibergs");
            force_cast(a->next) = b->next;
            force_cast(b->next) = a;
            force_cast(b->prev) = a->prev;
            force_cast(a->prev) = b;
        };
        // 相邻则节点?
        if (a___b) {
            swap_neibergs(ctrl1, ctrl2);
        }
        // 相邻则节点?
        else if (b___a) {
            swap_neibergs(ctrl2, ctrl1);
        }
        // 不相邻:交换前驱后驱
        else {
            std::swap(force_cast(ctrl1->prev), force_cast(ctrl2->prev));
            std::swap(force_cast(ctrl1->next), force_cast(ctrl2->next));
        }
#ifdef _DEBUG
        // 检查链表是否成环
        {
            auto count = m_cChildrenCount;
            auto debug_ctrl = m_pHead;
            while (debug_ctrl) {
                debug_ctrl = debug_ctrl->next;
                assert(count && "bad action 0 in swaping children");
                count--;
            }
            assert(!count && "bad action 1 in swaping children");
        }
        {
            auto count = m_cChildrenCount;
            auto debug_ctrl = m_pTail;
            while (debug_ctrl) {
                debug_ctrl = debug_ctrl->prev;
                assert(count && "bad action 2 in swaping children");
                count--;
            }
            assert(!count && "bad action 3 in swaping children");
        }
#endif
        // 刷新
        this->SetControlLayoutChanged();
        m_pWindow->Invalidate(this);
    }
    // 给予警告
    else {
        UIManager << DL_Warning
            << L"wanna to swap 2 children but just one"
            << LongUI::endl;
    }
}

// -------------------------- UIVerticalLayout -------------------------
// UIVerticalLayout 创建
auto LongUI::UIVerticalLayout::CreateControl(CreateEventType type, pugi::xml_node node) noexcept ->UIControl* {
    UIControl* pControl = nullptr;
    switch (type)
    {
    case LongUI::Type_Initialize:
        break;
    case LongUI::Type_Recreate:
        break;
    case LongUI::Type_Uninitialize:
        break;
    case_LongUI__Type_CreateControl:
        if (!node) {
            UIManager << DL_Hint << L"node null" << LongUI::endl;
        }
        // 申请空间
        pControl = CreateWidthCET<LongUI::UIVerticalLayout>(type, node);
        if (!pControl) {
            UIManager << DL_Error << L"alloc null" << LongUI::endl;
        }
    }
    return pControl;
}

// 更新子控件布局
void LongUI::UIVerticalLayout::RefreshLayout() noexcept {
    // 基本算法:
    // 1. 去除浮动控件影响
    // 2. 一次遍历, 检查指定高度的控件, 计算基本高度/宽度
    // 3. 计算实际高度/宽度
    {
        // 初始化
        float base_width = 0.f, base_height = 0.f, basic_weight = 0.f;
        // 第一次遍历
        for (auto ctrl : (*this)) {
            // 非浮点控件
            if (!(ctrl->flags & Flag_Floating)) {
                // 宽度固定?
                if (ctrl->flags & Flag_WidthFixed) {
                    base_width = std::max(base_width, ctrl->GetTakingUpWidth());
                }
                // 高度固定?
                if (ctrl->flags & Flag_HeightFixed) {
                    base_height += ctrl->GetTakingUpHeight();
                }
                // 未指定高度?
                else {
                    basic_weight += ctrl->weight;
                }
            }
        }
        // 带入控件本身宽度计算
        base_width = std::max(base_width, this->GetViewWidthZoomed());
        // 剩余高度富余
        register auto height_remain = std::max(this->GetViewHeightZoomed() - base_height, 0.f);
        // 单位权重高度
        auto height_in_unit_weight = basic_weight > 0.f ? height_remain / basic_weight : 0.f;
        // 基线Y
        float position_y = 0.f;
        // 第二次
        for (auto ctrl : (*this)) {
            // 浮点控
            if (ctrl->flags & Flag_Floating) continue;
            // 设置控件宽度
            if (!(ctrl->flags & Flag_WidthFixed)) {
                ctrl->SetWidth(base_width);
            }
            // 设置控件高度
            if (!(ctrl->flags & Flag_HeightFixed)) {
                ctrl->SetHeight(std::max(height_in_unit_weight * ctrl->weight, float(LongUIAutoControlMinSize)));
            }
            // 容器?
            // 不管如何, 修改!
            ctrl->SetControlLayoutChanged();
            ctrl->SetLeft(0.f);
            ctrl->SetTop(position_y);
            ctrl->world;
            //ctrl->RefreshWorld();
            position_y += ctrl->GetTakingUpHeight();
        }
        // 修改
        m_2fContentSize.width = base_width * m_2fZoom.width;
        m_2fContentSize.height = position_y * m_2fZoom.height;
    }
    this->RefreshWorld();
}

// UIVerticalLayout 关闭控件
void LongUI::UIVerticalLayout::cleanup() noexcept {
    delete this;
}

// -------------------------- UIHorizontalLayout -------------------------
// UIHorizontalLayout 创建
auto LongUI::UIHorizontalLayout::CreateControl(CreateEventType type, pugi::xml_node node) noexcept ->UIControl* {
    UIControl* pControl = nullptr;
    switch (type)
    {
    case LongUI::Type_Initialize:
        break;
    case LongUI::Type_Recreate:
        break;
    case LongUI::Type_Uninitialize:
        break;
    case_LongUI__Type_CreateControl:
        // 警告
        if (!node) {
            UIManager << DL_Hint << L"node null" << LongUI::endl;
        }
        // 申请空间
        pControl = CreateWidthCET<LongUI::UIHorizontalLayout>(type, node);
        // OOM
        if (!pControl) {
            UIManager << DL_Error << L"alloc null" << LongUI::endl;
        }
    }
    return pControl;
}


// 更新子控件布局
void LongUI::UIHorizontalLayout::RefreshLayout() noexcept {
    // 基本算法:
    // 1. 去除浮动控件影响
    // 2. 一次遍历, 检查指定高度的控件, 计算基本高度/宽度
    // 3. 计算实际高度/宽度
    {
        // 初始化
        float base_width = 0.f, base_height = 0.f;
        float basic_weight = 0.f;
        // 第一次
        for (auto ctrl : (*this)) {
            // 非浮点控件
            if (!(ctrl->flags & Flag_Floating)) {
                // 高度固定?
                if (ctrl->flags & Flag_HeightFixed) {
                    base_height = std::max(base_height, ctrl->GetTakingUpHeight());
                }
                // 宽度固定?
                if (ctrl->flags & Flag_WidthFixed) {
                    base_width += ctrl->GetTakingUpWidth();
                }
                // 未指定宽度?
                else {
                    basic_weight += ctrl->weight;
                }
            }
        }
        // 计算
        base_height = std::max(base_height, this->GetViewHeightZoomed());
        // 剩余宽度富余
        register auto width_remain = std::max(this->GetViewWidthZoomed() - base_width, 0.f);
        // 单位权重宽度
        auto width_in_unit_weight = basic_weight > 0.f ? width_remain / basic_weight : 0.f;
        // 基线X
        float position_x = 0.f;
        // 第二次
        for (auto ctrl : (*this)) {
            // 跳过浮动控件
            if (ctrl->flags & Flag_Floating) continue;
            // 设置控件高度
            if (!(ctrl->flags & Flag_HeightFixed)) {
                ctrl->SetHeight(base_height);
            }
            // 设置控件宽度
            if (!(ctrl->flags & Flag_WidthFixed)) {
                ctrl->SetWidth(std::max(width_in_unit_weight * ctrl->weight, float(LongUIAutoControlMinSize)));
            }
            // 不管如何, 修改!
            ctrl->SetControlLayoutChanged();
            ctrl->SetLeft(position_x);
            ctrl->SetTop(0.f);
            //ctrl->RefreshWorld();
            position_x += ctrl->GetTakingUpWidth();
        }
        // 修改
        //UIManager << DL_Hint << this << position_x << endl;
        m_2fContentSize.width = position_x;
        m_2fContentSize.height = base_height;
        // 已经处理
        this->ControlLayoutChangeHandled();
    }
}


// UIHorizontalLayout 关闭控件
void LongUI::UIHorizontalLayout::cleanup() noexcept {
    delete this;
}

// --------------------- Single Layout ---------------
// UISingle 析构函数
LongUI::UISingle::~UISingle() noexcept {
    assert(m_pChild && "UISingle must host a child");
    this->cleanup_child(m_pChild);
}

// UISingle: 事件处理
bool LongUI::UISingle::DoEvent(const LongUI::EventArgument& arg) noexcept {
    // 处理窗口事件
    if (arg.sender) {
        switch (arg.event)
        {
        case LongUI::Event::Event_TreeBulidingFinished:
            // 初始化边缘控件 
            Super::DoEvent(arg);
            // 初次完成空间树建立
            assert(m_pChild && "UISingle must host a child");
            m_pChild->DoEvent(arg);
            return true;
        }
    }
    return Super::DoEvent(arg);
}

// UISingle 重建
auto LongUI::UISingle::Recreate() noexcept -> HRESULT {
    HRESULT hr = S_OK;
    // 重建子控件
    assert(m_pChild && "UISingle must host a child");
    hr = m_pChild->Recreate();
    // 检查
    assert(SUCCEEDED(hr));
    return Super::Recreate();
}

// UISingle: 主景渲染
void LongUI::UISingle::render_chain_main() const noexcept {
    // 渲染帮助器
    Super::RenderHelper(this->begin(), this->end());
    // 父类主景
    Super::render_chain_main();
}

// UISingle: 渲染函数
void LongUI::UISingle::Render() const noexcept {
    // 背景渲染
    this->render_chain_background();
    // 主景渲染
    this->render_chain_main();
    // 前景渲染
    this->render_chain_foreground();
}

// UISingle: 刷新
void LongUI::UISingle::Update() noexcept {
    // 帮助器
    Super::UpdateHelper<Super>(this->begin(), this->end());
}

/// <summary>
/// Find the control via mouse point
/// </summary>
/// <param name="pt">The wolrd mouse point.</param>
/// <returns>the control pointer, maybe nullptr</returns>
auto LongUI::UISingle::FindChild(const D2D1_POINT_2F& pt) noexcept->UIControl* {
    // 父类(边缘控件)
    auto mctrl = Super::FindChild(pt);
    if (mctrl) return mctrl;
    assert(m_pChild && "UISingle must host a child");
    // 检查
    if (IsPointInRect(m_pChild->visible_rect, pt)) {
        return m_pChild;
    }
    return nullptr;
}


// UISingle: 推入最后
void LongUI::UISingle::PushBack(UIControl* child) noexcept {
    // 边界控件交给父类处理
    if (child && (child->flags & Flag_MarginalControl)) {
        Super::PushBack(child);
    }
    // 一般的就自己处理
    else {
        // 检查
#ifdef _DEBUG
        auto old = UIControl::GetPlaceholder();
        this->cleanup_child(old);
        if (old != m_pChild) {
            UIManager << DL_Warning
                << L"m_pChild exist:"
                << m_pChild
                << LongUI::endl;
        }
#endif
        // 移除之前的
        this->cleanup_child(m_pChild);
        this->after_insert(m_pChild = child);
    }
}

// UISingle: 仅移除
void LongUI::UISingle::RemoveJust(UIControl* child) noexcept {
    assert(m_pChild == child && "bad argment");
    this->cleanup_child(child);
    m_pChild = UIControl::GetPlaceholder();
    Super::RemoveJust(child);
}

// UISingle: 更新布局
void LongUI::UISingle::RefreshLayout() noexcept {
    // 浮动控件?
    if (m_pChild->flags & Flag_Floating) {
        // 更新
        m_2fContentSize.width = m_pChild->GetWidth() * m_2fZoom.width;
        m_2fContentSize.height = m_pChild->GetHeight() * m_2fZoom.height;
    }
    // 一般控件
    else {
        // 设置控件宽度
        if (!(m_pChild->flags & Flag_WidthFixed)) {
            m_pChild->SetWidth(this->GetViewWidthZoomed());
        }
        // 设置控件高度
        if (!(m_pChild->flags & Flag_HeightFixed)) {
            m_pChild->SetWidth(this->GetViewHeightZoomed());
        }
        // 不管如何, 修改!
        m_pChild->SetControlLayoutChanged();
        m_pChild->SetLeft(0.f);
        m_pChild->SetTop(0.f);
    }
}

// UISingle 清理
void LongUI::UISingle::cleanup() noexcept {
    delete this;
}

// UISingle 创建空间
auto LongUI::UISingle::CreateControl(CreateEventType type, pugi::xml_node node) noexcept ->UIControl* {
    UIControl* pControl = nullptr;
    switch (type)
    {
    case LongUI::Type_Initialize:
        break;
    case LongUI::Type_Recreate:
        break;
    case LongUI::Type_Uninitialize:
        break;
    case_LongUI__Type_CreateControl:
        // 警告
        if (!node) {
            UIManager << DL_Hint << L"node null" << LongUI::endl;
        }
        // 申请空间
        pControl = CreateWidthCET<LongUI::UISingle>(type, node);
        // OOM
        if (!pControl) {
            UIManager << DL_Error << L"alloc null" << LongUI::endl;
        }
    }
    return pControl;
}