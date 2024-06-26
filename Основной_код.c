#include "lodepng.c"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#define KERNEL_SIZE 5
#define Epsilon 19

double kernel[KERNEL_SIZE][KERNEL_SIZE] = {
        {1.0/256, 4.0/256, 6.0/256, 4.0/256, 1.0/256},
        {4.0/256, 16.0/256, 24.0/256, 16.0/256, 4.0/256},
        {6.0/256, 24.0/256, 36.0/256, 24.0/256, 6.0/256},
        {4.0/256, 16.0/256, 24.0/256, 16.0/256, 4.0/256},
        {1.0/256, 4.0/256, 6.0/256, 4.0/256, 1.0/256}
};

char* load_png_file(const char *filename, int *width, int *height) {
    unsigned char *image = NULL;
    int error = lodepng_decode32_file(&image, width, height, filename);
    if (error) {
        printf("error %u: %s\n", error, lodepng_error_text(error));
        return NULL;
    }

    return (image);
}

struct Node {
    int rank;
    unsigned char RED, GREEN, BLUE, WHITE;
    struct Node *up, *down, *left, *right;
    struct Node *parent;

};
typedef struct Node Node;

typedef struct {
    int x, y;
} Point;


void applyFilter(unsigned char *image, int width, int height) {
    unsigned char *result = malloc(width * height * 4 * sizeof(unsigned char));

    for (int y = 2; y < height - 2; y++){
        for (int x = 2; x < width - 2; x++) {
            double red = 0.0, green = 0.0, blue = 0.0;
            for (int dy = -2; dy <= 2; dy++) {
                for (int dx = -2; dx <= 2; dx++) {
                    int index = ((y+dy) * width + (x+dx)) * 4;
                    red += image[index] * kernel[dy + 2][dx + 2];
                    green += image[index + 1] * kernel[dy + 2][dx + 2];
                    blue += image[index + 2] * kernel[dy + 2][dx + 2];
                }
            }
            int resultIndex = (y * width + x) * 4;
            result[resultIndex] = (unsigned char)round(red);
            result[resultIndex + 1] = (unsigned char)round(green);
            result[resultIndex + 2] = (unsigned char)round(blue);
            result[resultIndex + 3] = image[resultIndex + 3];
        }
    }

    for (int i = 0; i < width * height * 4; i++) {
        image[i] = result[i];
    }

    free(result);
}


Node* find(Node* circle) {
    return circle->parent != circle ? find(circle->parent) : circle->parent;
}

void union_set(Node* x, Node* y) {
    Node* par_X = find(x);
    Node* par_Y = find(y);

    double color_difference = sqrt(pow(x->RED - y->RED, 2) * 3);

    if (x->RED < 10) return;

    if (par_X != par_Y && color_difference < Epsilon) {
        if (par_X->rank > par_Y->rank)  par_Y->parent = par_X;
        else {
            par_X->parent = par_Y;
            if (par_X->rank == par_Y->rank) par_Y->rank += 1;
        }
    }
}


void find_set(Node* nodes, int width, int height) {
    int x, y;

    for (y = 0; y < height; ++y) {
        for (x = 0; x < width; ++x) {

            Node* node = &nodes[y * width + x];

            if (node->up) union_set(node, node->up);

            if (node->down) union_set(node, node->down);

            if (node->left) union_set(node, node->left);

            if (node->right) union_set(node, node->right);

        }
    }
}

void color_components(Node* nodes, int width, int height) {

    unsigned char* output_image = malloc(width * height * 4 * sizeof(unsigned char));

    int *component_sizes = calloc(width * height, sizeof(int)), i, I;

    for (i = 0; i < width * height; ++i) {

        Node* par = find(&nodes[i]);
        if (par == &nodes[i]) {
            if (component_sizes[i] < 2) {
                par->RED = 0;
                par->GREEN = 0;
                par->BLUE = 0;
            } else {
                par->RED = rand() % 256;
                par->GREEN = rand() % 256;
                par->BLUE = rand() % 256;
            }
        }

        I = 4 * i;

        output_image[I++] = par->RED;
        output_image[I++] = par->GREEN;
        output_image[I++] = par->BLUE;
        output_image[I++] = 255;

        component_sizes[par - nodes]++;
    }
    // Cþäà íóæíî âñòàâèòü ññûëêó íà png'øíûé ôàéë, ãäå áóäåò îòâåò
    char* output_filename = "output_2.png";

    lodepng_encode32_file(output_filename, output_image, width, height);

    free(output_image);
    free(component_sizes);
}

void perona_malik(unsigned char *image, int width, int height, float dt, float K, int N) {
    float dx, dy, dxx, dyy, dxy, grad;
    float cN, cS, cW, cE;

    unsigned char *result = malloc(width * height * sizeof(unsigned char));

    for (int t = 0; t < N; t++) {
        for (int i = 1; i < height-1; i++) {
            for (int j = 1; j < width-1; j++) {
                dx = (image[4 * ((i+1)*width+j)] - image[ 4 * ((i-1)*width+j)]) / 2.0;
                dy = (image[4 *(i*width+(j+1))] - image[4 *(i*width+(j-1))]) / 2.0;
                dxx = image[4 *((i+1)*width+j)] - 2*image[4 *(i*width+j)] + image[4 *((i-1)*width+j)];
                dyy = image[4 *(i*width+(j+1))] - 2*image[4 *(i*width+j)] + image[4 *(i*width+(j-1))];
                dxy = (image[4 *((i+1)*width+(j+1))] - image[4 *((i+1)*width+(j-1))] - image[4 *((i-1)*width+(j+1))] + image[4 *((i-1)*width+(j-1))]) / 4.0;
                grad = dx*dx + dy*dy;

                cN = exp(-(dx*dx + dxy*dxy) / (K*K));
                cS = exp(-(dx*dx + dxy*dxy) / (K*K));
                cW = exp(-(dy*dy + dxy*dxy) / (K*K));
                cE = exp(-(dy*dy + dxy*dxy) / (K*K));

                result[(i*width+j)] = image[4 *(i*width+j)] + dt * (cN*dxx + cS*dxx + cW*dyy + cE*dyy);
            }
        }
        for (int i = 0; i < width * height; i++) {
            int resultIndex = i * 4;
            image[resultIndex] = (unsigned char) result[i];
            image[resultIndex + 1] = (unsigned char) result[i];
            image[resultIndex + 2] = (unsigned char) result[i];
            image[resultIndex + 3] = image[resultIndex + 3];

        }
    }

    free(result);
}

void applySobelFilter(unsigned char *image, int width, int height) {

    int gx[3][3] = {{-1, 0, 1},
                    {-2, 0, 2},
                    {-1, 0, 1}};

    int gy[3][3] = {{1, 2, 1},
                    {0, 0, 0},
                    {-1, -2, -1}};


    unsigned char* result = malloc(width * height * 4 * sizeof(unsigned char));
    int y, x, dy, dx, summ_X, summ_Y, index , gray, res_Index, i;

    for (y = 1; y < height - 1; ++y) {
        for (x = 1; x < width - 1; ++x) {

            summ_X = 0, summ_Y = 0;

            for (dy = -1; dy <= 1; ++dy) {
                for (dx = -1; dx <= 1; ++dx) {

                    index = ((y+dy) * width + (x+dx)) * 4;
                    gray = (image[index] + image[index + 1] + image[index + 2]) / 3;

                    summ_X += gx[dy + 1][dx + 1] * gray;
                    summ_Y += gy[dy + 1][dx + 1] * gray;
                }
            }

            int magnitude = sqrt(summ_X * summ_X + summ_Y * summ_Y);
            if (magnitude > 255) magnitude = 255;

            res_Index = (y * width + x) * 4;

            result[res_Index] = (unsigned char)magnitude;
            result[res_Index + 1] = (unsigned char)magnitude;
            result[res_Index + 2] = (unsigned char)magnitude;
            result[res_Index + 3] = image[res_Index + 3];
        }
    }

    for (i = 0; i < width * height * 4; i++)  image[i] = result[i];


    free(result);
}


void floodFill(unsigned char* image, int x, int y, int newColor1, int newColor2, int newColor3, int oldColor, int width, int height) {
    int dx[] = {-1, 0, 1, 0};
    int dy[] = {0, 1, 0, -1};

    Point* stack = malloc(width * height * 4 * sizeof(Point));
    long top = 0;

    stack[top++] = (Point){x, y};

    while(top > 0) {
        Point p = stack[--top];

        if(p.x < 0 || p.x >= width || p.y < 0 || p.y >= height)
            continue;

        int resultIndex = (p.y * width + p.x) * 4;
        if(image[resultIndex] > oldColor)
            continue;

        image[resultIndex] = newColor1;
        image[resultIndex + 1] = newColor2;
        image[resultIndex + 2] = newColor3;


        for(int i = 0; i < 4; i++) {
            int nx = p.x + dx[i];
            int ny = p.y + dy[i];
            int nIndex = (ny * width + nx) * 4;
            if(nx > 0 && nx < width && ny > 0 && ny < height && image[nIndex] <= oldColor) {
                stack[top++] = (Point){nx, ny};
            }
        }
    }
    free(stack);
}

void colorComponents(unsigned char* image, int width, int height, int epsilon) {
    int color1 = epsilon * 2;
    int color2 = epsilon * 2;
    int color3 = epsilon * 2;
    int cnt = 0;
    for(int y = 1; y < height - 1; y++) {
        for(int x = 1; x < width - 1; x++) {
            if(image[4 * (y * width + x)] < epsilon) {
                cnt++;
                floodFill(image, x, y, color1, color2, color3, epsilon, width, height);

            }
            color1 = rand() % (255 - epsilon * 2) + epsilon * 2;
            color2 = rand() % (255 - epsilon * 2) + epsilon * 2;
            color3 = rand() % (255 - epsilon * 2) + epsilon * 2;
        }
    }
    char *output_filename = "output3.png";
    lodepng_encode32_file(output_filename, image, width, height);
    printf("%d", cnt);
}


int main() {
    int width = 0, hight = 0, N = 200;
    float dt = 0.15, K = 1.0;

    // Cþäà íóæíî âñòàâèòü ññûëêó íà èçíà÷àëüíûé png'øíûé ôàéë
    char *filename = "input.png";

    char *picture = load_png_file(filename, &width, &hight);

    applyFilter(picture, width, hight);
    applySobelFilter(picture, width, hight);

    perona_malik(picture, width, hight, dt, K, N);



    colorComponents(picture, width, hight, 5);


    free(picture);

    return 0;
}
