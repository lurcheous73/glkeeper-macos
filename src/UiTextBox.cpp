#include "stdafx.h"
#include "UiTextBox.h"
#include "UiWidgetManager.h"
#include "UiRenderContext.h"
#include "UiManager.h"
#include "FontManager.h"
#include "TextManager.h"

UiTextBox::UiTextBox() : UiTextBox("textbox")
{
    mInteractive = false;
}

UiTextBox::UiTextBox(const std::string& widgetClassName)
    : UiWidget(widgetClassName)
    , mHorzAlignment(eTextHorzAlignment_Left)
    , mTextColorDefault(COLOR_WHITE)
    , mTextColorHovered(COLOR_WHITE)
    , mTextColorDisabled(COLOR_LIGHT_GRAY)
    , mTextColor(COLOR_WHITE)
{
}

UiTextBox::UiTextBox(const UiTextBox& sourceWidget)
    : UiWidget(sourceWidget)
    , mHorzAlignment(sourceWidget.mHorzAlignment)
    , mVertAlignment(sourceWidget.mVertAlignment)
    , mTextColorDefault(sourceWidget.mTextColorDefault)
    , mTextColorHovered(sourceWidget.mTextColorHovered)
    , mTextColorDisabled(sourceWidget.mTextColorDisabled)
    , mTextColor(sourceWidget.mTextColor)
    , mTextContent(sourceWidget.mTextContent)
    , mTextFont(sourceWidget.mTextFont)
    , mTextBatchDirty(true) // force
    , mStringId(sourceWidget.mStringId)
    , mTextTableId(sourceWidget.mTextTableId)
    , mFontScale(sourceWidget.mFontScale)
{
}

UiTextBox::~UiTextBox()
{
}

UiTextBox* UiTextBox::CloneSelf() const
{
    UiTextBox* clone = new UiTextBox(*this);
    return clone;
}

void UiTextBox::Refresh()
{
    if (mTextBatchDirty)
    {
        mTextBatchDirty = false;
        RecomptuteCache();
    }
}

void UiTextBox::SetText(const std::wstring& text)
{
    if (mTextContent == text) 
        return;

    mTextContent = text;
    mTextTableId = TextTableId_Null;
    InvalidateCache();
}

void UiTextBox::SetStringId(TextTableId textTableId, int stringId)
{
    if ((mStringId == stringId) && (mTextTableId == textTableId)) 
        return;

    mTextTableId = textTableId;
    mStringId = stringId;
    InvalidateCache();
}

void UiTextBox::SetTextFont(const std::string& textFontName)
{
    Font* uiFont = nullptr;
    
    if (!textFontName.empty())
    {
        uiFont = gFontManager.GetFont(textFontName);
        cxx_assert(uiFont);
    }

    if (uiFont != mTextFont)
    {
        mTextFont = uiFont;
        InvalidateCache();
    }
}

void UiTextBox::SetTextColor(Color32 textColor)
{
    if (mTextColorDefault == textColor)
        return;

    mTextColorDefault = textColor;
    RefreshColor();
}

void UiTextBox::SetTextAlignment(eTextHorzAlignment alignmentMode)
{
    if (mHorzAlignment == alignmentMode)
        return;

    mHorzAlignment = alignmentMode;
    InvalidateCache();
}   

void UiTextBox::SetTextAlignment(eTextVertAlignment alignmentMode)
{
    if (mVertAlignment == alignmentMode)
        return;

    mVertAlignment = alignmentMode;
    InvalidateCache();
}

void UiTextBox::Deserialize(const JsonElement& jsonElement)
{
    UiWidget::Deserialize(jsonElement);

    // font
    if (JsonElement pathNode = jsonElement.FindElement("font"))
    {
        std::string fontName = pathNode.GetValueString();
        if (fontName.length())
        {
            SetTextFont(fontName);
            cxx_assert(mTextFont);
        }
    }

    // horz align mode
    if (JsonElement alignmentNode = jsonElement.FindElement("horz_align"))
    {
        const std::string alignment = alignmentNode.GetValueString();
        bool isSuccess = cxx::parse_enum(alignment.c_str(), mHorzAlignment);
        if (!isSuccess)
        {
            gConsole.LogMessage(eLogLevel_Warning, "Unknown text alignment mode '%s'", alignment.c_str());
        }
        cxx_assert(isSuccess);
    }

    if (JsonElement textNode = jsonElement.FindElement("text"))
    {
        std::string_view textValue = textNode.GetValueString();
        if (!cxx::string_to_wide_string(textValue, mTextContent))
        {
            cxx_assert(false);
        }
    }

    JsonQuery(jsonElement, "color", mTextColorDefault);
    JsonQuery(jsonElement, "color_hovered", mTextColorHovered);
    JsonQuery(jsonElement, "color_disabled", mTextColorDisabled);
    if (JsonQuery(jsonElement, "string_id", mStringId))
    {
        mTextTableId = TextTableId_Main;
    }
    JsonQuery(jsonElement, "font_scale", mFontScale);

    RefreshColor();
    InvalidateCache();
}

void UiTextBox::RenderSelf(UiRenderContext& uiRenderContext)
{
    if (mTextFont == nullptr)
        return;

    Refresh();
    uiRenderContext.DrawTextQuads(mTextFont, mTextBatch.data(), mTextBatch.size());
}

void UiTextBox::HandleSizeChanged(const Point2D& prevSize)
{
    InvalidateCache();
}

void UiTextBox::HandleMouseEnter()
{
    RefreshColor();
}

void UiTextBox::HandleMouseLeave()
{
    RefreshColor();
}

void UiTextBox::HandleEnabledChanged()
{
    RefreshColor();
}

void UiTextBox::HandleVisibleChanged()
{
    RefreshColor();
}

void UiTextBox::HandleInputEvent(MouseButtonInputEvent& inputEvent)
{
    if (inputEvent.IsButtonPressed(MBUTTON_LEFT))
    {
        inputEvent.SetConsumed();
        // notify
        const UiEvent_OnPress eventDesc (MBUTTON_LEFT, inputEvent.mMousePosition);
        NotifyListeners(eventDesc);
    }
}

void UiTextBox::RecomptuteCache()
{
    mTextBatch.clear();

    if (mTextFont == nullptr) 
        return;

    if (mTextTableId != TextTableId_Null)
    {
        mTextContent = gTexts.GetString(mTextTableId, mStringId);
    }

    if (std::getenv("KEEPER_FRONTEND_TRACE") && (mStringId == 71 || mStringId == 1675 || mStringId == 28))
    {
        std::fprintf(stderr, "DK2TEXT id=%d table=%d len=%zu font=%s visible=%d enabled=%d codes=",
            mStringId, static_cast<int>(mTextTableId), mTextContent.size(),
            mTextFont ? mTextFont->GetName().c_str() : "(null)",
            IsVisibleInHierarchy() ? 1 : 0, IsEnabledInHierarchy() ? 1 : 0);
        for (size_t ci = 0; ci < std::min<size_t>(mTextContent.size(), 12); ++ci)
            std::fprintf(stderr, "%X%s", static_cast<unsigned int>(mTextContent[ci]), (ci + 1 < std::min<size_t>(mTextContent.size(), 12)) ? "," : "");
        std::fprintf(stderr, "\n");
    }

    if (mTextContent.empty()) 
        return;

    Rect2D localBounds = GetLocalBounds();
    if ((localBounds.w > 0) || (localBounds.h > 0))
    {
        Font::BuildTextMeshParams params;
        params.mBounds = localBounds;
        params.mHorzAlign = mHorzAlignment;
        params.mVertAlign = mVertAlignment;
        params.mFontScale = mFontScale;
        mTextFont->BuildTextMesh(mTextContent, params, mTextColor, mTextBatch);
    }
    else
    {
        mTextFont->BuildTextMesh(mTextContent, localBounds.GetPosition(), mTextColor, mTextBatch);
    }
    if (std::getenv("KEEPER_FRONTEND_TRACE") && (mStringId == 71 || mStringId == 1675 || mStringId == 28))
        std::fprintf(stderr, "DK2TEXT id=%d quads=%zu bounds=%d,%d,%d,%d\n",
            mStringId, mTextBatch.size(), localBounds.x, localBounds.y, localBounds.w, localBounds.h);
}

void UiTextBox::InvalidateCache()
{
    mTextBatchDirty = true;
}

void UiTextBox::RefreshColor()
{
    const Color32 currentColor = !IsEnabledInHierarchy() ? mTextColorDisabled :
        (IsHovered() ? mTextColorHovered : mTextColorDefault);

    if (mTextColor == currentColor) 
        return;

    mTextColor = currentColor;

    // update text batch
    for (Quad2D& textQuad: mTextBatch)
    {
        for (Vertex2D& vertex: textQuad.mPoints)
        {
            vertex.mColor = mTextColor;
        }
    }
}
