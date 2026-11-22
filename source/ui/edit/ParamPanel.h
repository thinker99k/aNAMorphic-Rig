#pragma once

#include "engine/Block.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <memory>
#include <vector>

namespace rig
{
    class RigModel;

    // 영역 묶음: 같은 영역을 다루는 항목(예: B 와 B. Freq, L. Cut 전원과 노브)을 옅은 둥근 사각 바탕 안에 왼쪽부터 놓는다.
    // 항목은 묶음의 자식이 되고(소유는 패널), 세로 가운데. 패널은 묶음만 왼쪽부터 놓는다
    class ParamGroup final : public juce::Component
    {
    public:
        static constexpr int padding = 8;      // 바탕 안쪽 좌우 여백
        static constexpr int knobGap = 12;     // 노브 사이
        static constexpr int powerGap = 8;     // 전원 버튼과 노브 사이 (합류 화면과 같다)

        // gapBefore = 앞 항목과의 간격 (첫 항목은 무시). height 0 = 묶음 높이 전체 (노브는 스스로 세로 가운데)
        void addItem (juce::Component& item, int width, int gapBefore, int height = 0)
        {
            items.push_back ({ &item, width, items.empty() ? 0 : gapBefore, height });
            addAndMakeVisible (item);
        }

        int getIdealWidth() const
        {
            int w = 2 * padding;
            for (const auto& i : items)
                w += i.gapBefore + i.width;
            return w;
        }

        void paint (juce::Graphics& g) override
        {
            const auto r = getLocalBounds().toFloat().reduced (0.5f);
            g.setColour (juce::Colours::black.withAlpha (0.2f));
            g.fillRoundedRectangle (r, cornerRadius);
            g.setColour (juce::Colours::white.withAlpha (0.08f));
            g.drawRoundedRectangle (r, cornerRadius, 1.0f);
        }

        void resized() override
        {
            auto area = getLocalBounds().reduced (padding, 0);

            for (const auto& i : items)
            {
                area.removeFromLeft (i.gapBefore);
                const auto column = area.removeFromLeft (i.width);
                i.component->setBounds (i.height > 0 ? column.withSizeKeepingCentre (i.width, juce::jmin (i.height, column.getHeight()))
                                                     : column);
            }
        }

    private:
        static constexpr float cornerRadius = 6.0f;

        struct Item { juce::Component* component; int width, gapBefore, height; };
        std::vector<Item> items;
    };

    // ParamBar 왼쪽에 들어가는, 블록 종류별 파라미터 화면
    class ParamPanel : public juce::Component
    {
    public:
        virtual void refresh() = 0;   // 모델 값(선택된 블록의 현재 씬) -> 화면

    protected:
        static constexpr int groupMargin = 6;   // 묶음 위아래 여백 (파라미터 바 높이 83 이면 묶음 71)
        static constexpr int groupGap = 16;     // 묶음 사이

        // 묶음들을 왼쪽부터 놓는다
        void layoutGroups (std::initializer_list<ParamGroup*> groups)
        {
            auto area = getLocalBounds().reduced (0, groupMargin);

            for (auto* group : groups)
            {
                group->setBounds (area.removeFromLeft (group->getIdealWidth()));
                area.removeFromLeft (groupGap);
            }
        }
    };

    // 블록 종류에 맞는 패널. 파라미터가 없는 종류는 nullptr.
    // 블록 종류를 추가할 때는 여기에 한 경우를 더한다
    std::unique_ptr<ParamPanel> createParamPanel (BlockType type, RigModel& model);
}
