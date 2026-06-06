class TileMap {
    float z;
    unsigned int tid;
    int width;
    int height;
    unsigned char *map;

public:
    TileMap(int w, int h, unsigned char initWith) {
        map = new unsigned char[w * h];
        width = w;
        height = h;
        z = 0.0f;
        tid = 0;
        for (int index = 0; index < width * height; ++index) {
            map[index] = initWith;
        }
    }

    ~TileMap() {
        delete[] map;
    }

    unsigned char *getMap() {
        return map;
    }

    int getWidth() {
        return width;
    }

    int getHeight() {
        return height;
    }

    int getTile(int col, int row) {
        return map[col + row * width];
    }

    void setTile(int col, int row, unsigned char tile) {
        map[col + row * width] = tile;
    }

    int getTileSet() {
        return tid;
    }

    float getZ() {
        return z;
    }

    void setZ(float newZ) {
        z = newZ;
    }

    void setTid(int newTid) {
        tid = newTid;
    }
};