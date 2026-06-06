#ifndef DIAMOND_VIEW_H
#define DIAMOND_VIEW_H

#include <cmath>

#include "TilemapView.h"

class DiamondView : public TilemapView {
public:
    DiamondView(float originX = -1.0f, float originY = -1.0f)
        : mapWidth(0), mapHeight(0), originX(originX), originY(originY) {
    }

    void setMapSize(int width, int height) {
        mapWidth = width;
        mapHeight = height;
    }

    void computeDrawPosition(
        int col,
        int row,
        float tw,
        float th,
        float &targetx,
        float &targety
    ) const override {
        const float horizontalOffset = mapHeight > 0 ? (mapHeight - 1) * tw * 0.5f : 0.0f;
        targetx = originX + horizontalOffset + (col - row) * tw * 0.5f;
        targety = originY + (col + row) * th * 0.5f;
    }

    void computeMouseMap(
        int &col,
        int &row,
        float tw,
        float th,
        float mx,
        float my
    ) const override {
        const float horizontalOffset = mapHeight > 0 ? (mapHeight - 1) * tw * 0.5f : 0.0f;
        const float localX = mx - originX - horizontalOffset;
        const float localY = my - originY;
        const float sum = localY / (th * 0.5f);
        const float diff = localX / (tw * 0.5f);

        col = static_cast<int>(std::floor((sum + diff) * 0.5f));
        row = static_cast<int>(std::floor((sum - diff) * 0.5f));
    }

    void computeTileWalking(int &col, int &row, int direction) const override {
        switch (direction) {
        case DIRECTION_NORTH:
            --col;
            break;
        case DIRECTION_SOUTH:
            ++col;
            break;
        case DIRECTION_EAST:
            --row;
            break;
        case DIRECTION_WEST:
            ++row;
            break;
        case DIRECTION_NORTHEAST:
            --col;
            --row;
            break;
        case DIRECTION_NORTHWEST:
            --col;
            ++row;
            break;
        case DIRECTION_SOUTHEAST:
            ++col;
            --row;
            break;
        case DIRECTION_SOUTHWEST:
            ++col;
            ++row;
            break;
        default:
            break;
        }
    }

private:
    int mapWidth;
    int mapHeight;
    float originX;
    float originY;
};

#endif