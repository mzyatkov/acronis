#include "io_png.h"
#include <limits.h>
#define PX_DEL_COUNT 1 //количество пикселей для удаления за один раз
EXTERN int width, height;
EXTERN png_byte color_type;
EXTERN png_byte bit_depth;
EXTERN png_bytep *row_pointers;
int norm(png_bytep a, png_bytep b)
{
    int ans = 0;
    for (int i = 0; i < 3; i++)
    {
        ans += (a[i] - b[i]) * (a[i] - b[i]);
    }
    return ans;
}

int px_weight(int x, int y)
{
    int ans = 0;
    if (x != 0)
    {
        ans += norm(&row_pointers[y][(x - 1) * 4], &row_pointers[y][x * 4]);
    }
    if (y != 0)
    {
        ans += norm(&row_pointers[y - 1][x * 4], &row_pointers[y][x * 4]);
    }
    if (x < width - 1)
    {
        ans += norm(&row_pointers[y][(x + 1) * 4], &row_pointers[y][x * 4]);
    }
    if (y < height - 1)
    {
        ans += norm(&row_pointers[y + 1][x * 4], &row_pointers[y][x * 4]);
    }
    return ans;
}

int **alloc_weights()
{
    int **weights = (int **)malloc(sizeof(int *) * height);
    assert(weights);
    for (int i = 0; i < height; i++)
    {
        weights[i] = (int *)malloc(sizeof(int) * width);
        assert(weights[i]);
    }
    return weights;
}
void dealloc_weights(int **weights)
{
    for (int i = 0; i < height; i++)
    {
        free(weights[i]);
    }
    free(weights);
}
int min(int a, int b, int c)
{
    if (a <= b && a <= c)
    {
        return a;
    }
    if (b <= c)
    {
        return b;
    }
    return c;
}
int safe_min(int *w_row, int x)
{
    int a = INT_MAX, b = w_row[x], c = INT_MAX;
    if (x != 0)
    {
        a = w_row[x - 1];
    }
    if (x != width - 1)
    {
        c = w_row[x + 1];
    }
    return min(a, b, c);
}
void erase(int *row, int x)
{
    for (int i = x; i < width - PX_DEL_COUNT; i++)
    {
        row[i] = row[i + PX_DEL_COUNT];
    }
}
void erase_px(png_bytep row, int x)
{
    for (int i = x; i < width - PX_DEL_COUNT; i++)
    {
        // png_bytep px = &(row[i * 4]);
        // png_bytep px_next = &(row[(i+1) * 4]);
        // for (int j = 0; j < 4; j++) {
        //     px[j] = px_next[j];//можно ускорить копирование
        // }
        int *px = (int *)&(row[i * 4]);
        int *px_next = (int *)&(row[(i + PX_DEL_COUNT) * 4]);
        *px = *px_next;
    }
}

void reduce_path(int **weights)
{
    // заменяем веса точек на веса путей до точки сверху вниз
    for (int y = 1; y < height; y++)
    {
        int *w_row = weights[y];
        int *w_prev_row = weights[y - 1];
        for (int x = 0; x < width; x++)
        {
            int *w_px = &w_row[x];
            *w_px += safe_min(w_prev_row, x);
        }
    }
    // восстанавливаем минимальный путь и сокращаем изображение
    int start_min = INT_MAX, start_x = -1;
    for (int x = 0; x < width; x++)
    {
        if (start_min > weights[height - 1][x])
        {
            start_min = weights[height - 1][x];
            start_x = x;
        }
    }
    int next_x = start_x;
    for (int y = height - 1; y >= 1; y--)
    {
        int *w_row = weights[y];
        int *w_next_row = weights[y - 1];
        png_bytep row = row_pointers[y];

        start_x = next_x;
        erase(w_row, start_x);
        erase_px(row, start_x);
        int a = INT_MAX, b = w_next_row[start_x], c = INT_MAX;

        if (start_x != 0)
        {
            a = w_next_row[start_x - 1];
            if (a < b)
            {
                next_x = start_x - 1;
            }
        }
        if (start_x != width - 1)
        {
            c = w_next_row[start_x + 1];
            if (c < b && c < a)
            {
                next_x = start_x + 1;
            }
        }
    }
    erase(weights[0], next_x);
    erase_px(row_pointers[0], next_x);
    width -= PX_DEL_COUNT; //уменьшаем глобальную ширину
}
void process_png_file(int final_width)
{
    int **weights = alloc_weights();
    //заполняем таблицу с весами пикселей
    for (int y = 0; y < height; y++)
    {
        png_bytep row = row_pointers[y];
        int *w_row = weights[y];
        for (int x = 0; x < width; x++)
        {
            int *w_px = &w_row[x];
            *w_px = px_weight(x, y);
        }
    }
    int save_width = width;
    while (width > final_width)
    {
        reduce_path(weights);
    }
    dealloc_weights(weights);
}

int reduce_image(char *filename, int final_width)
{
    read_png_file(filename);
    assert(final_width > 0 && final_width <= width);
    process_png_file(final_width);
    printf("width=%d\n", width);
    write_png_file("out.png");
}
int main()
{
    printf("asdf\n");
    reduce_image("salivan.png", 200);
}
