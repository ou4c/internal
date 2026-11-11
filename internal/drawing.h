#pragma once
#include <string>
#include <vector>
#include <memory>
#include <cstdint>

namespace DrawingAPI {
    struct Color3 {
        float r = 1.0f;
        float g = 1.0f;
        float b = 1.0f;
    };

    struct Vector2 {
        float x = 0.0f;
        float y = 0.0f;
    };

    class BaseType {
    public:
        virtual ~BaseType() = default;
        virtual void Render() {}
        virtual void Destroy() { m_Visible = false; }

        bool m_Visible = true;
        int m_ZIndex = 1;
        float m_Transparency = 1.0f;
        Color3 m_Color;
    };

    class LineType : public BaseType {
    public:
        Vector2 m_From;
        Vector2 m_To;
        float m_Thickness = 1.0f;
    };

    class CircleType : public BaseType {
    public:
        Vector2 m_Position;
        float m_Radius = 10.0f;
        float m_Thickness = 1.0f;
        int m_NumSides = 32;
        bool m_Filled = false;
    };

    class SquareType : public BaseType {
    public:
        Vector2 m_Position;
        Vector2 m_Size;
        float m_Thickness = 1.0f;
        bool m_Filled = false;
    };

    class TextType : public BaseType {
    public:
        std::string m_Text;
        Vector2 m_Position;
        float m_Size = 14.0f;
        bool m_Center = false;
        bool m_Outline = false;
        Color3 m_OutlineColor;
        int m_Font = 0;
    };

    class TriangleType : public BaseType {
    public:
        Vector2 m_PointA;
        Vector2 m_PointB;
        Vector2 m_PointC;
        float m_Thickness = 1.0f;
        bool m_Filled = false;
    };

    class QuadType : public BaseType {
    public:
        Vector2 m_PointA;
        Vector2 m_PointB;
        Vector2 m_PointC;
        Vector2 m_PointD;
        float m_Thickness = 1.0f;
        bool m_Filled = false;
    };

    class ImageType : public BaseType {
    public:
        std::string m_Data;
        Vector2 m_Position;
        Vector2 m_Size;
        float m_Rounding = 0.0f;
    };

    class FontType : public BaseType {
    public:
        int m_FontId = 0;
    };

    class DrawingManager {
    public:
        static DrawingManager& Get();
        void Add(std::shared_ptr<BaseType> obj);
        void Remove(std::shared_ptr<BaseType> obj);
        void Clear();
        void RenderAll();

    private:
        std::vector<std::shared_ptr<BaseType>> m_Objects;
    };
}
