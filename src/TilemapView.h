#ifndef TILEMAP_VIEW_H
#define TILEMAP_VIEW_H

#define DIRECTION_NORTH 1
#define DIRECTION_SOUTH 2
#define DIRECTION_EAST 3
#define DIRECTION_WEST 4
#define DIRECTION_NORTHEAST 5
#define DIRECTION_NORTHWEST 6
#define DIRECTION_SOUTHEAST 7
#define DIRECTION_SOUTHWEST 8

class TilemapView {
public:
    virtual ~TilemapView() = default;

    virtual void computeDrawPosition(
        int col,
        int row,
        float tw,
        float th,
        float &targetx,
        float &targety
    ) const = 0;

    virtual void computeMouseMap(
        int &col,
        int &row,
        float tw,
        float th,
        float mx,
        float my
    ) const = 0;

    virtual void computeTileWalking(int &col, int &row, int direction) const = 0;
};

#endif