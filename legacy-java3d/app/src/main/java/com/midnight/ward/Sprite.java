package com.midnight.ward;

/** 世界中的精灵（物品 / 陈设 / 怨灵），用于 billboard 渲染与拾取判定。 */
public class Sprite {

    public static final int TYPE_KEY = 0;      // 病房钥匙
    public static final int TYPE_CARD = 1;     // 房卡
    public static final int TYPE_TORCH = 2;    // 手电筒
    public static final int TYPE_BKEY = 3;     // 地下室钥匙
    public static final int TYPE_CROWBAR = 4;  // 撬棍
    public static final int TYPE_BED = 5;      // 病床（陈设）
    public static final int TYPE_GHOST = 6;    // 怨灵（氛围）

    public float x, y;
    public int floor;
    public int type;
    public boolean alive = true;
    public boolean pickable;
    public float alpha = 1f;
    public float height;   // 世界高度（墙高 = 1.0）
    public float aspect;   // 宽 / 高

    public Sprite(int floor, float x, float y, int type) {
        this.floor = floor;
        this.x = x;
        this.y = y;
        this.type = type;
        switch (type) {
            case TYPE_KEY:
            case TYPE_BKEY:
                height = 0.26f;
                aspect = 1.0f;
                pickable = true;
                break;
            case TYPE_CARD:
                height = 0.20f;
                aspect = 1.0f;
                pickable = true;
                break;
            case TYPE_TORCH:
                height = 0.22f;
                aspect = 1.0f;
                pickable = true;
                break;
            case TYPE_CROWBAR:
                height = 0.30f;
                aspect = 1.0f;
                pickable = true;
                break;
            case TYPE_BED:
                height = 0.42f;
                aspect = 1.6f;
                break;
            case TYPE_GHOST:
                height = 0.92f;
                aspect = 0.667f;
                break;
            default:
                height = 0.3f;
                aspect = 1.0f;
                break;
        }
    }
}
