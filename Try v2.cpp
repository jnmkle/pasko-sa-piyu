// ============================================================================
// CS0045 - Computer Graphics and Visual Computing (Final Project)
// Project Title: Tamaraw Beats - FEU Manila Grandstand 3D Concert & Light Show
// Libraries: FreeGLUT, GLEW, OpenGL
// ============================================================================

#define _USE_MATH_DEFINES
#include <GL/glew.h>
#include <GL/freeglut.h>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// ============================================================================
// GLOBAL STATE VARIABLES
// ============================================================================

// Window dimensions
int winWidth = 1280;
int winHeight = 720;

// Camera orbit & zoom variables (Default view frames Stage Front + FEU Grandstand)
float camYaw = 190.0f;
float camPitch = 21.0f;
float camDistance = 27.0f;
float camPanX = 0.0f;
int lastMouseX = 0, lastMouseY = 0;
bool isDraggingLeft = false;
bool isDraggingRight = false;

// Animation & Transformation variables
float simTime = 0.0f;
float spotSweepAngle = 0.0f;   // Rotation for sweeping moving-head spotlights
float crystalSpinAngle = 0.0f; // Rotation for VBO centerpiece
float riserHeight = 0.0f;      // Translation for center drum riser
float bassPulse = 1.0f;        // Scaling for line arrays, 4 floor subs & 2D EQ
bool bandJamActive = true;     // Toggles musicians' motion, bass pulse & 2D EQ
bool hazeMachinesOn = true;    // Toggles Left & Right stage haze machines [M]
bool showHUD = true;           // Toggles compact 2D console overlay [H]

// Lighting & Show Modes
int lightShowMode = 1;     // 1 = Smooth Sweep, 2 = Party Chase, 3 = Manual Freeze
bool houseLightOn = true;  // GL_LIGHT0: Field/Grandstand ambient light [0]
bool spotCyanOn = true;    // GL_LIGHT1: Outer Cyan moving-head bank [1]
bool spotMagentaOn = true; // GL_LIGHT2: Inner Magenta moving-head bank [2]
bool spotAmberOn = true;   // GL_LIGHT3: Center Amber moving-head spot [3]
bool spotBeamsOn = true;   // Back-truss FEU Green/Gold beam bank [4]
bool floorSpotsOn = true;  // 8 Stage-Floor moving-head beam bank [5]
bool strobeOn = true;      // GL_LIGHT4: High-speed Strobe units [F]
bool ledParsOn = true;     // GL_LIGHT5: RGB Wash LED PAR cans [P]
bool stormyOn = true;      // GL_LIGHT6: Stormy Blinder / Lightning bursts [T]
bool ledBarsOn = true;     // GL_LIGHT7: Multi-segment LED Pixel Bars [E]

// Texture & VBO handles
GLuint backdropTextureID = 0;
GLuint liveFeedTextureID = 0;
GLuint crystalVBO = 0;
int crystalVertexCount = 0;

// Custom vertex structure for the VBO
struct Vertex
{
    float x, y, z;    // 3D Position
    float nx, ny, nz; // Surface Normal
    float u, v;       // Texture Coordinates
};

// ============================================================================
// HELPER: CAPPED CYLINDER & 3D STROKE TEXT
// ============================================================================
void drawCylinder(float baseRadius, float topRadius, float height, int slices)
{
    GLUquadric *quad = gluNewQuadric();
    gluQuadricNormals(quad, GLU_SMOOTH);
    gluQuadricTexture(quad, GLU_TRUE);

    gluCylinder(quad, baseRadius, topRadius, height, slices, 1);

    glPushMatrix();
    glRotatef(180.0f, 1.0f, 0.0f, 0.0f);
    gluDisk(quad, 0.0, baseRadius, slices, 1);
    glPopMatrix();

    glPushMatrix();
    glTranslatef(0.0f, 0.0f, height);
    gluDisk(quad, 0.0, topRadius, slices, 1);
    glPopMatrix();

    gluDeleteQuadric(quad);
}

void drawStrokeText3D(const char *text, float scale)
{
    glPushMatrix();
    glScalef(scale, scale, scale);
    for (const char *p = text; *p; p++)
    {
        glutStrokeCharacter(GLUT_STROKE_ROMAN, *p);
    }
    glPopMatrix();
}

// ============================================================================
// REQUIREMENT 6: TEXTURE MAPPING (24-bit BMP Loader + Procedural Fallbacks)
// ============================================================================
bool tryLoadBMPData(const char *filename, std::vector<unsigned char> &outData,
                    unsigned int &width, unsigned int &height)
{
    FILE *file = fopen(filename, "rb");
    if (!file)
        return false;

    unsigned char header[54];
    if (fread(header, 1, 54, file) != 54 || header[0] != 'B' || header[1] != 'M')
    {
        fclose(file);
        return false;
    }

    unsigned int dataPos = *(int *)&(header[0x0A]);
    unsigned int imageSize = *(int *)&(header[0x22]);
    width = *(int *)&(header[0x12]);
    height = *(int *)&(header[0x16]);

    if (imageSize == 0)
        imageSize = width * height * 3;
    if (dataPos == 0)
        dataPos = 54;

    outData.resize(imageSize);
    fseek(file, dataPos, SEEK_SET);
    fread(outData.data(), 1, imageSize, file);
    fclose(file);
    return true;
}

GLuint loadConcertTexture(const char *filename)
{
    GLuint texID;
    glGenTextures(1, &texID);
    glBindTexture(GL_TEXTURE_2D, texID);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    std::vector<unsigned char> bmpData;
    unsigned int w = 0, h = 0;
    if (tryLoadBMPData(filename, bmpData, w, h))
    {
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, w, h, 0,
                     GL_BGR_EXT, GL_UNSIGNED_BYTE, bmpData.data());
        printf("Loaded external texture: %s (%ux%u)\n", filename, w, h);
        return texID;
    }

    // Fallback: Generates an FEU Green & Gold LED Stage Equalizer Texture
    printf("Note: '%s' not found. Using generated FEU Green & Gold LED backdrop texture.\n", filename);
    const int W = 256, H = 256;
    unsigned char fallbackData[W * H * 3];
    for (int y = 0; y < H; y++)
    {
        for (int x = 0; x < W; x++)
        {
            int idx = (y * W + x) * 3;
            bool gridLine = (x % 32 < 2) || (y % 32 < 2);
            bool goldChevron = (abs((x % 64) - 32) + abs((y % 64) - 32)) < 14;
            if (gridLine)
            {
                fallbackData[idx] = 245;
                fallbackData[idx + 1] = 195;
                fallbackData[idx + 2] = 25;
            }
            else if (goldChevron)
            {
                fallbackData[idx] = 20;
                fallbackData[idx + 1] = 210;
                fallbackData[idx + 2] = 110;
            }
            else
            {
                fallbackData[idx] = 8;
                fallbackData[idx + 1] = (unsigned char)(45 + (y * 55 / H));
                fallbackData[idx + 2] = (unsigned char)(28 + (x * 35 / W));
            }
        }
    }
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, W, H, 0, GL_RGB, GL_UNSIGNED_BYTE, fallbackData);
    return texID;
}

// Texture for the Left & Right Rising Live-Feed IMAG Screens
GLuint loadLiveFeedTexture(const char *filename)
{
    GLuint texID;
    glGenTextures(1, &texID);
    glBindTexture(GL_TEXTURE_2D, texID);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    std::vector<unsigned char> bmpData;
    unsigned int w = 0, h = 0;
    if (tryLoadBMPData(filename, bmpData, w, h))
    {
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, w, h, 0,
                     GL_BGR_EXT, GL_UNSIGNED_BYTE, bmpData.data());
        printf("Loaded external live-feed texture: %s (%ux%u)\n", filename, w, h);
        return texID;
    }

    // Procedural Live Camera Feed View (Stage Spotlights, Performer Silhouette & Broadcast HUD)
    const int W = 256, H = 256;
    unsigned char data[W * H * 3];
    for (int y = 0; y < H; y++)
    {
        for (int x = 0; x < W; x++)
        {
            int idx = (y * W + x) * 3;
            float nx = (float)x / W;
            float ny = (float)y / H;

            int r = (int)(16 + 24 * ny);
            int g = (int)(48 + 55 * (1.0f - fabsf(nx - 0.5f)));
            int b = (int)(38 + 45 * ny);

            float beam1 = fabsf((nx - 0.25f) - (1.0f - ny) * 0.35f);
            float beam2 = fabsf((nx - 0.75f) + (1.0f - ny) * 0.35f);
            if (beam1 < 0.08f)
            {
                g += 95;
                b += 120;
            }
            if (beam2 < 0.08f)
            {
                r += 110;
                g += 85;
            }

            float dx = (nx - 0.50f);
            float dyHead = (ny - 0.56f);
            bool inHead = (dx * dx + dyHead * dyHead) < (0.075f * 0.075f);
            bool inTorso = (fabsf(dx) < (0.13f - (ny - 0.25f) * 0.12f)) && (ny >= 0.14f && ny <= 0.48f);
            if (inHead || inTorso)
            {
                r = 12;
                g = 18;
                b = 15;
            }

            bool borderFrame = ((nx < 0.06f || nx > 0.94f) && (ny < 0.14f || ny > 0.86f)) ||
                               ((ny < 0.05f || ny > 0.95f) && (nx < 0.16f || nx > 0.84f));
            if (borderFrame)
            {
                r = 250;
                g = 210;
                b = 40;
            }

            float liveDx = nx - 0.14f;
            float liveDy = ny - 0.88f;
            if ((liveDx * liveDx + liveDy * liveDy) < (0.032f * 0.032f))
            {
                r = 245;
                g = 30;
                b = 35;
            }

            if (y % 4 == 0)
            {
                r = (r * 85) / 100;
                g = (g * 85) / 100;
                b = (b * 85) / 100;
            }

            data[idx] = (unsigned char)(r > 255 ? 255 : r);
            data[idx + 1] = (unsigned char)(g > 255 ? 255 : g);
            data[idx + 2] = (unsigned char)(b > 255 ? 255 : b);
        }
    }
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, W, H, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
    return texID;
}

// ============================================================================
// REQUIREMENT 7: VERTEX BUFFER OBJECT (VBO) SETUP
// Custom 3D 16-facet Concert Diamond/Prism suspended beneath the dome apex
// ============================================================================
void initCrystalVBO()
{
    std::vector<Vertex> vertices;
    const int slices = 8;
    const float radius = 0.85f;
    const float halfHeight = 1.25f;

    auto computeNormal = [](float x1, float y1, float z1,
                            float x2, float y2, float z2,
                            float x3, float y3, float z3,
                            float &nx, float &ny, float &nz)
    {
        float ux = x2 - x1, uy = y2 - y1, uz = z2 - z1;
        float vx = x3 - x1, vy = y3 - y1, vz = z3 - z1;
        nx = uy * vz - uz * vy;
        ny = uz * vx - ux * vz;
        nz = ux * vy - uy * vx;
        float len = std::sqrt(nx * nx + ny * ny + nz * nz);
        if (len > 0.0001f)
        {
            nx /= len;
            ny /= len;
            nz /= len;
        }
    };

    for (int i = 0; i < slices; i++)
    {
        float a1 = (2.0f * (float)M_PI * i) / slices;
        float a2 = (2.0f * (float)M_PI * (i + 1)) / slices;

        float x1 = radius * cosf(a1), z1 = radius * sinf(a1);
        float x2 = radius * cosf(a2), z2 = radius * sinf(a2);
        float nx, ny, nz;

        computeNormal(0.0f, halfHeight, 0.0f, x2, 0.0f, z2, x1, 0.0f, z1, nx, ny, nz);
        vertices.push_back({0.0f, halfHeight, 0.0f, nx, ny, nz, 0.5f, 1.0f});
        vertices.push_back({x2, 0.0f, z2, nx, ny, nz, 1.0f, 0.0f});
        vertices.push_back({x1, 0.0f, z1, nx, ny, nz, 0.0f, 0.0f});

        computeNormal(0.0f, -halfHeight, 0.0f, x1, 0.0f, z1, x2, 0.0f, z2, nx, ny, nz);
        vertices.push_back({0.0f, -halfHeight, 0.0f, nx, ny, nz, 0.5f, 0.0f});
        vertices.push_back({x1, 0.0f, z1, nx, ny, nz, 0.0f, 1.0f});
        vertices.push_back({x2, 0.0f, z2, nx, ny, nz, 1.0f, 1.0f});
    }

    crystalVertexCount = (int)vertices.size();

    glGenBuffers(1, &crystalVBO);
    glBindBuffer(GL_ARRAY_BUFFER, crystalVBO);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex), vertices.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void drawCrystalVBO()
{
    glBindBuffer(GL_ARRAY_BUFFER, crystalVBO);

    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_NORMAL_ARRAY);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);

    glVertexPointer(3, GL_FLOAT, sizeof(Vertex), (void *)0);
    glNormalPointer(GL_FLOAT, sizeof(Vertex), (void *)(3 * sizeof(float)));
    glTexCoordPointer(2, GL_FLOAT, sizeof(Vertex), (void *)(6 * sizeof(float)));

    glDrawArrays(GL_TRIANGLES, 0, crystalVertexCount);

    glDisableClientState(GL_TEXTURE_COORD_ARRAY);
    glDisableClientState(GL_NORMAL_ARRAY);
    glDisableClientState(GL_VERTEX_ARRAY);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

// ============================================================================
// LIGHTING PULSE HELPERS (Strobe & Stormy Timing Logic)
// ============================================================================
bool isStrobeFlashing()
{
    if (!strobeOn)
        return false;
    int tick = (int)(simTime * 18.0f);
    return (tick % 4 == 0);
}

bool isStormyFlashing()
{
    if (!stormyOn)
        return false;
    float cycle = fmodf(simTime, 2.4f);
    return (cycle > 1.85f && cycle < 1.94f) || (cycle > 2.02f && cycle < 2.16f);
}

// ============================================================================
// REQUIREMENT 5: MULTI-FIXTURE STAGE LIGHTING SETUP (GL_LIGHT0 .. GL_LIGHT7)
// ============================================================================
void setupLighting()
{
    glEnable(GL_LIGHTING);
    glEnable(GL_COLOR_MATERIAL);
    glEnable(GL_NORMALIZE);
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);

    GLfloat matSpecular[] = {0.85f, 0.85f, 0.9f, 1.0f};
    GLfloat matShininess[] = {60.0f};
    glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, matSpecular);
    glMaterialfv(GL_FRONT_AND_BACK, GL_SHININESS, matShininess);

    // GL_LIGHT0: FEU Manila Field & Grandstand Evening Ambient Light
    if (houseLightOn)
    {
        glEnable(GL_LIGHT0);
        GLfloat ambient0[] = {0.25f, 0.26f, 0.31f, 1.0f};
        GLfloat diffuse0[] = {0.50f, 0.50f, 0.58f, 1.0f};
        GLfloat pos0[] = {0.0f, 22.0f, -14.0f, 1.0f};
        glLightfv(GL_LIGHT0, GL_AMBIENT, ambient0);
        glLightfv(GL_LIGHT0, GL_DIFFUSE, diffuse0);
        glLightfv(GL_LIGHT0, GL_POSITION, pos0);
    }
    else
    {
        glDisable(GL_LIGHT0);
    }

    // Rotate stage-mounted light sources 180 degrees to match the rotated stage
    glPushMatrix();
    glRotatef(180.0f, 0.0f, 1.0f, 0.0f);

    bool partyBlink = (lightShowMode != 2) || (((int)(simTime * 10.0f) % 2) == 0);

    auto configureSpotlight = [&](GLenum lightID, bool enabled,
                                  float x, float y, float z,
                                  float yawDeg, float pitchDeg,
                                  float r, float g, float b, float cutoff)
    {
        if (!enabled)
        {
            glDisable(lightID);
            return;
        }
        glEnable(lightID);
        GLfloat pos[] = {x, y, z, 1.0f};
        GLfloat diffuse[] = {r, g, b, 1.0f};
        GLfloat specular[] = {r, g, b, 1.0f};

        float yawRad = yawDeg * (float)M_PI / 180.0f;
        float pitchRad = pitchDeg * (float)M_PI / 180.0f;
        GLfloat dir[] = {
            sinf(yawRad) * cosf(pitchRad),
            -cosf(pitchRad),
            sinf(pitchRad)};

        glLightfv(lightID, GL_POSITION, pos);
        glLightfv(lightID, GL_DIFFUSE, diffuse);
        glLightfv(lightID, GL_SPECULAR, specular);
        glLightfv(lightID, GL_SPOT_DIRECTION, dir);
        glLightf(lightID, GL_SPOT_CUTOFF, cutoff);
        glLightf(lightID, GL_SPOT_EXPONENT, 10.0f);
    };

    // GL_LIGHT1..3: Overhead Sweeping Moving-Head Spotlights
    configureSpotlight(GL_LIGHT1, spotCyanOn && partyBlink,
                       -5.2f, 7.75f, -0.9f, spotSweepAngle, 20.0f, 0.0f, 0.95f, 1.0f, 32.0f);
    configureSpotlight(GL_LIGHT2, spotMagentaOn && partyBlink,
                       5.2f, 7.75f, -0.9f, -spotSweepAngle, 20.0f, 1.0f, 0.15f, 0.85f, 32.0f);
    configureSpotlight(GL_LIGHT3, spotAmberOn && partyBlink,
                       0.0f, 8.75f, -0.9f, sinf(simTime * 1.5f) * 22.0f, 22.0f, 1.0f, 0.78f, 0.1f, 32.0f);

    // GL_LIGHT4: High-Intensity Strobe Lights [F]
    if (isStrobeFlashing())
    {
        glEnable(GL_LIGHT4);
        GLfloat strobePos[] = {0.0f, 8.35f, 1.2f, 1.0f};
        GLfloat strobeCol[] = {1.0f, 1.0f, 1.0f, 1.0f};
        glLightfv(GL_LIGHT4, GL_POSITION, strobePos);
        glLightfv(GL_LIGHT4, GL_DIFFUSE, strobeCol);
        glLightfv(GL_LIGHT4, GL_SPECULAR, strobeCol);
    }
    else
    {
        glDisable(GL_LIGHT4);
    }

    // GL_LIGHT5: Color-Cycling LED PAR Wash [P]
    if (ledParsOn)
    {
        glEnable(GL_LIGHT5);
        float parR = 0.5f + 0.5f * sinf(simTime * 1.7f);
        float parG = 0.5f + 0.5f * sinf(simTime * 1.7f + 2.09f);
        float parB = 0.5f + 0.5f * sinf(simTime * 1.7f + 4.18f);
        GLfloat parPos[] = {0.0f, 7.75f, 2.2f, 1.0f};
        GLfloat parDiff[] = {parR * 0.85f, parG * 0.85f, parB * 0.85f, 1.0f};
        GLfloat parDir[] = {0.0f, -0.85f, -0.52f};
        glLightfv(GL_LIGHT5, GL_POSITION, parPos);
        glLightfv(GL_LIGHT5, GL_DIFFUSE, parDiff);
        glLightfv(GL_LIGHT5, GL_SPOT_DIRECTION, parDir);
        glLightf(GL_LIGHT5, GL_SPOT_CUTOFF, 60.0f);
    }
    else
    {
        glDisable(GL_LIGHT5);
    }

    // GL_LIGHT6: Stormy Audience Blinder / Lightning Burst [T]
    if (isStormyFlashing())
    {
        glEnable(GL_LIGHT6);
        GLfloat stormPos[] = {0.0f, 8.55f, 2.4f, 1.0f};
        GLfloat stormCol[] = {0.75f, 0.92f, 1.0f, 1.0f};
        glLightfv(GL_LIGHT6, GL_POSITION, stormPos);
        glLightfv(GL_LIGHT6, GL_DIFFUSE, stormCol);
        glLightfv(GL_LIGHT6, GL_SPECULAR, stormCol);
    }
    else
    {
        glDisable(GL_LIGHT6);
    }

    // GL_LIGHT7: Front LED Pixel Bar Footlight Glow [E]
    if (ledBarsOn)
    {
        glEnable(GL_LIGHT7);
        GLfloat barPos[] = {0.0f, 1.65f, 2.9f, 1.0f};
        GLfloat barDiff[] = {0.25f, 0.65f, 0.45f, 1.0f};
        glLightfv(GL_LIGHT7, GL_POSITION, barPos);
        glLightfv(GL_LIGHT7, GL_DIFFUSE, barDiff);
    }
    else
    {
        glDisable(GL_LIGHT7);
    }

    glPopMatrix();
}

// ============================================================================
// LOCATION: FEU MANILA GRANDSTAND & ATHLETIC FIELD ENVIRONMENT
// ============================================================================
void drawFEUManilaGrandstand()
{
    // 1. FEU Manila Open Athletic Grass Field
    glPushMatrix();
    glTranslatef(0.0f, -0.05f, 12.0f);
    glScalef(56.0f, 0.10f, 64.0f);
    glColor3f(0.07f, 0.24f, 0.10f);
    glutSolidCube(1.0);
    glPopMatrix();

    // 2. White Athletic Track & Field Pitch Markings
    glDisable(GL_LIGHTING);
    glColor3f(0.82f, 0.86f, 0.82f);
    glLineWidth(2.0f);
    glBegin(GL_LINE_LOOP);
    glVertex3f(-18.5f, 0.01f, -6.5f);
    glVertex3f(18.5f, 0.01f, -6.5f);
    glVertex3f(18.5f, 0.01f, 25.5f);
    glVertex3f(-18.5f, 0.01f, 25.5f);
    glEnd();
    glBegin(GL_LINES);
    glVertex3f(-18.5f, 0.01f, 10.5f);
    glVertex3f(18.5f, 0.01f, 10.5f);
    glEnd();
    glEnable(GL_LIGHTING);

    // 3. Main FEU Manila Covered Grandstand Bleachers (In Background behind Stage)
    glPushMatrix();
    glTranslatef(0.0f, 0.0f, 29.5f);

    const float gsWidth = 34.0f;
    const int numTiers = 10;

    glPushMatrix();
    glTranslatef(0.0f, 0.65f, -3.2f);
    glScalef(gsWidth, 1.3f, 0.5f);
    glColor3f(0.88f, 0.87f, 0.82f);
    glutSolidCube(1.0);
    glPopMatrix();
    glPushMatrix();
    glTranslatef(0.0f, 1.35f, -3.22f);
    glScalef(gsWidth, 0.16f, 0.54f);
    glColor3f(0.95f, 0.76f, 0.12f);
    glutSolidCube(1.0);
    glPopMatrix();

    for (int t = 0; t < numTiers; t++)
    {
        float tierY = 1.45f + t * 0.48f;
        float tierZ = -2.6f + t * 0.72f;

        glPushMatrix();
        glTranslatef(0.0f, tierY * 0.5f, tierZ);
        glScalef(gsWidth, tierY, 0.74f);
        glColor3f(0.76f, 0.76f, 0.74f);
        glutSolidCube(1.0);
        glPopMatrix();

        glPushMatrix();
        glTranslatef(0.0f, tierY + 0.12f, tierZ);
        glScalef(gsWidth - 0.6f, 0.12f, 0.42f);
        if (t % 3 == 1)
            glColor3f(0.95f, 0.76f, 0.12f);
        else
            glColor3f(0.04f, 0.38f, 0.18f);
        glutSolidCube(1.0);
        glPopMatrix();
    }

    glPushMatrix();
    glTranslatef(0.0f, 4.8f, 4.6f);
    glScalef(gsWidth + 0.4f, 9.6f, 0.6f);
    glColor3f(0.90f, 0.89f, 0.84f);
    glutSolidCube(1.0);
    glPopMatrix();

    for (int c = -4; c <= 4; c++)
    {
        glPushMatrix();
        glTranslatef(c * 4.1f, 4.8f, 4.15f);
        glScalef(0.55f, 9.6f, 0.55f);
        glColor3f(0.04f, 0.36f, 0.18f);
        glutSolidCube(1.0);
        glPopMatrix();
    }

    glPushMatrix();
    glTranslatef(0.0f, 7.4f, 3.2f);
    glScalef(14.0f, 2.2f, 2.4f);
    glColor3f(0.05f, 0.34f, 0.17f);
    glutSolidCube(1.0);
    glPopMatrix();

    glPushMatrix();
    glTranslatef(0.0f, 9.7f, 0.2f);
    glRotatef(-6.0f, 1.0f, 0.0f, 0.0f);
    glScalef(gsWidth + 1.6f, 0.35f, 10.2f);
    glColor3f(0.04f, 0.35f, 0.17f);
    glutSolidCube(1.0);
    glPopMatrix();

    glPushMatrix();
    glTranslatef(0.0f, 9.15f, -4.75f);
    glScalef(gsWidth + 1.6f, 0.42f, 0.28f);
    glColor3f(0.95f, 0.76f, 0.12f);
    glutSolidCube(1.0);
    glPopMatrix();

    glDisable(GL_LIGHTING);
    glPushMatrix();
    glTranslatef(8.2f, 9.07f, -4.92f);
    glRotatef(180.0f, 0.0f, 1.0f, 0.0f);
    glColor3f(0.03f, 0.24f, 0.11f);
    glLineWidth(2.2f);
    drawStrokeText3D("FEU MANILA GRANDSTAND", 0.0076f);
    glPopMatrix();
    glEnable(GL_LIGHTING);
    glPopMatrix();

    // 4. Flanking FEU Campus Buildings & Green/Gold Perimeter Banners
    for (int side = -1; side <= 1; side += 2)
    {
        float bx = side * 22.5f;

        glPushMatrix();
        glTranslatef(bx, 5.5f, 8.0f);
        glScalef(4.5f, 11.0f, 34.0f);
        glColor3f(0.86f, 0.85f, 0.80f);
        glutSolidCube(1.0);
        glPopMatrix();

        glPushMatrix();
        glTranslatef(bx, 11.2f, 8.0f);
        glScalef(4.9f, 0.45f, 34.4f);
        glColor3f(0.04f, 0.38f, 0.18f);
        glutSolidCube(1.0);
        glPopMatrix();

        glDisable(GL_LIGHTING);
        glColor3f(0.96f, 0.85f, 0.48f);
        for (int floor = 0; floor < 3; floor++)
        {
            for (int w = -3; w <= 3; w++)
            {
                glPushMatrix();
                glTranslatef(bx - side * 2.28f, 3.0f + floor * 2.8f, 8.0f + w * 4.2f);
                glScalef(0.05f, 1.2f, 2.0f);
                glutSolidCube(1.0);
                glPopMatrix();
            }
        }
        glEnable(GL_LIGHTING);

        for (int b = 0; b < 4; b++)
        {
            float bz = -2.0f + b * 6.5f;
            glPushMatrix();
            glTranslatef(side * 19.2f, 0.85f, bz);
            glScalef(0.15f, 1.7f, 5.8f);
            if (b % 2 == 0)
                glColor3f(0.04f, 0.38f, 0.18f);
            else
                glColor3f(0.95f, 0.76f, 0.12f);
            glutSolidCube(1.0);
            glPopMatrix();
        }
    }
}

// ============================================================================
// DOME TENT TRUSS & TENSIONED CANOPY STRUCTURE
// ============================================================================
void drawDomeTentTruss()
{
    const float halfSpanX = 10.6f;
    const float baseY = 4.15f;
    const float archRiseY = 5.35f;
    const float zRibs[5] = {2.7f, 0.9f, -0.9f, -2.7f, -4.5f};
    const int segments = 24;

    float cornerX[2] = {-halfSpanX, halfSpanX};
    float cornerZ[2] = {zRibs[0], zRibs[4]};
    glColor3f(0.60f, 0.64f, 0.70f);

    for (int ix = 0; ix < 2; ix++)
    {
        for (int iz = 0; iz < 2; iz++)
        {
            glPushMatrix();
            glTranslatef(cornerX[ix], baseY * 0.5f, cornerZ[iz]);
            glScalef(0.36f, baseY, 0.36f);
            glutSolidCube(1.0);
            glPopMatrix();
        }
    }

    for (int ix = 0; ix < 2; ix++)
    {
        glPushMatrix();
        glTranslatef(cornerX[ix], baseY, (zRibs[0] + zRibs[4]) * 0.5f);
        glScalef(0.32f, 0.32f, zRibs[0] - zRibs[4] + 0.36f);
        glutSolidCube(1.0);
        glPopMatrix();
    }

    for (int r = 0; r < 5; r++)
    {
        float z = zRibs[r];
        glLineWidth(2.5f);

        for (int chord = 0; chord < 2; chord++)
        {
            float radiusOffset = (chord == 0) ? 0.0f : -0.30f;
            glBegin(GL_LINE_STRIP);
            for (int s = 0; s <= segments; s++)
            {
                float theta = (float)M_PI * (1.0f - (float)s / segments);
                float x = (halfSpanX + radiusOffset) * cosf(theta);
                float y = baseY + (archRiseY + radiusOffset) * sinf(theta);
                glVertex3f(x, y, z - 0.14f);
            }
            glEnd();

            glBegin(GL_LINE_STRIP);
            for (int s = 0; s <= segments; s++)
            {
                float theta = (float)M_PI * (1.0f - (float)s / segments);
                float x = (halfSpanX + radiusOffset) * cosf(theta);
                float y = baseY + (archRiseY + radiusOffset) * sinf(theta);
                glVertex3f(x, y, z + 0.14f);
            }
            glEnd();
        }

        glLineWidth(1.5f);
        glBegin(GL_LINES);
        for (int s = 0; s < segments; s++)
        {
            float t1 = (float)M_PI * (1.0f - (float)s / segments);
            float t2 = (float)M_PI * (1.0f - (float)(s + 1) / segments);

            float x1Out = halfSpanX * cosf(t1), y1Out = baseY + archRiseY * sinf(t1);
            float x2In = (halfSpanX - 0.30f) * cosf(t2), y2In = baseY + (archRiseY - 0.30f) * sinf(t2);
            float x1In = (halfSpanX - 0.30f) * cosf(t1), y1In = baseY + (archRiseY - 0.30f) * sinf(t1);
            float x2Out = halfSpanX * cosf(t2), y2Out = baseY + archRiseY * sinf(t2);

            glVertex3f(x1Out, y1Out, z + 0.14f);
            glVertex3f(x2In, y2In, z + 0.14f);
            glVertex3f(x1In, y1In, z - 0.14f);
            glVertex3f(x2Out, y2Out, z - 0.14f);
            glVertex3f(x1Out, y1Out, z - 0.14f);
            glVertex3f(x1Out, y1Out, z + 0.14f);
        }
        glEnd();
    }

    int purlinAngles[5] = {4, 8, 12, 16, 20};
    for (int p = 0; p < 5; p++)
    {
        float theta = (float)M_PI * (1.0f - (float)purlinAngles[p] / segments);
        float px = (halfSpanX - 0.15f) * cosf(theta);
        float py = baseY + (archRiseY - 0.15f) * sinf(theta);

        glPushMatrix();
        glTranslatef(px, py, (zRibs[0] + zRibs[4]) * 0.5f);
        glScalef(0.16f, 0.16f, zRibs[0] - zRibs[4]);
        glutSolidCube(1.0);
        glPopMatrix();
    }

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glColor4f(0.10f, 0.18f, 0.16f, 0.58f);

    for (int r = 0; r < 4; r++)
    {
        float z1 = zRibs[r] + 0.16f;
        float z2 = zRibs[r + 1] - 0.16f;
        glBegin(GL_QUAD_STRIP);
        for (int s = 0; s <= segments; s++)
        {
            float theta = (float)M_PI * (1.0f - (float)s / segments);
            float nx = cosf(theta);
            float ny = sinf(theta);
            float x = (halfSpanX + 0.08f) * nx;
            float y = baseY + (archRiseY + 0.08f) * ny;

            glNormal3f(-nx, -ny, 0.0f);
            glVertex3f(x, y, z1);
            glVertex3f(x, y, z2);
        }
        glEnd();
    }
    glDisable(GL_BLEND);
}

// ============================================================================
// LED WALLS:
// 1) Center 9x24 Main Backdrop LED Wall
// 2) Left & Right 9x12 Rising Live-Feed IMAG LED Walls
// ============================================================================
void drawLedCabinetPanel(float width, float height, int cols, int rows,
                         GLuint textureID, float texScaleX)
{
    float halfW = width * 0.5f;
    float halfH = height * 0.5f;

    glPushMatrix();
    glTranslatef(0.0f, 0.0f, -0.11f);
    glScalef(width + 0.18f, height + 0.18f, 0.18f);
    glColor3f(0.10f, 0.10f, 0.13f);
    glutSolidCube(1.0);
    glPopMatrix();

    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, textureID);
    glColor3f(1.0f, 1.0f, 1.0f);
    glBegin(GL_QUADS);
    glNormal3f(0.0f, 0.0f, 1.0f);
    glTexCoord2f(0.0f, 0.0f);
    glVertex3f(-halfW, -halfH, 0.0f);
    glTexCoord2f(texScaleX, 0.0f);
    glVertex3f(halfW, -halfH, 0.0f);
    glTexCoord2f(texScaleX, 1.0f);
    glVertex3f(halfW, halfH, 0.0f);
    glTexCoord2f(0.0f, 1.0f);
    glVertex3f(-halfW, halfH, 0.0f);
    glEnd();
    glDisable(GL_TEXTURE_2D);

    glDisable(GL_LIGHTING);
    glColor4f(0.05f, 0.05f, 0.08f, 0.65f);
    glLineWidth(1.0f);
    glBegin(GL_LINES);
    for (int c = 1; c < cols; c++)
    {
        float x = -halfW + (width * c) / cols;
        glVertex3f(x, -halfH, 0.01f);
        glVertex3f(x, halfH, 0.01f);
    }
    for (int r = 1; r < rows; r++)
    {
        float y = -halfH + (height * r) / rows;
        glVertex3f(-halfW, y, 0.01f);
        glVertex3f(halfW, y, 0.01f);
    }
    glEnd();
    glEnable(GL_LIGHTING);
}

void drawCenterLedWall9x24(float stageY)
{
    const float wallH = 3.15f; // 9 ft proportional height
    const float midW = 8.40f;  // 24 ft proportional width (9x24)
    const float centerY = stageY + 0.15f + wallH * 0.5f;

    glPushMatrix();
    glTranslatef(0.0f, centerY, -4.15f);
    drawLedCabinetPanel(midW, wallH, 8, 3, backdropTextureID, 2.0f);
    glPopMatrix();
}

void drawRisingLiveFeedLedWalls()
{
    const float wallH = 3.15f;   // 9 ft proportional height
    const float sideW = 4.20f;   // 12 ft proportional width (9x12)
    const float towerX = 13.85f; // Clear lateral offset outside the Â±10.6f dome truss
    const float towerZ = 0.85f;  // Support truss sits behind the LED screen plane
    const float towerH = 7.45f;  // Terminates cleanly behind the top of the LED cabinet
    const float screenY = 5.85f;
    const float screenZ = 1.65f; // Mounted strictly in front of the truss tower

    for (int side = -1; side <= 1; side += 2)
    {
        float xPos = side * towerX;

        // Vertical Support Truss Columns (Strictly behind the LED cabinet at z = towerZ)
        glColor3f(0.58f, 0.62f, 0.68f);
        for (int col = -1; col <= 1; col += 2)
        {
            glPushMatrix();
            glTranslatef(xPos + col * 0.95f, towerH * 0.5f, towerZ);
            glScalef(0.22f, towerH, 0.22f);
            glutSolidCube(1.0);
            glPopMatrix();
        }

        // Horizontal & Diagonal Truss Bracing
        for (int lvl = 0; lvl < 6; lvl++)
        {
            float yBase = 0.65f + lvl * 1.20f;
            glPushMatrix();
            glTranslatef(xPos, yBase, towerZ);
            glScalef(2.05f, 0.13f, 0.16f);
            glutSolidCube(1.0);
            glPopMatrix();

            glPushMatrix();
            glTranslatef(xPos, yBase + 0.58f, towerZ);
            glRotatef((lvl % 2 == 0) ? 26.0f : -26.0f, 0.0f, 0.0f, 1.0f);
            glScalef(2.15f, 0.09f, 0.09f);
            glutSolidCube(1.0);
            glPopMatrix();
        }

        // Ground Ballast Base Plate
        glPushMatrix();
        glTranslatef(xPos, 0.12f, towerZ);
        glScalef(2.8f, 0.24f, 1.6f);
        glColor3f(0.20f, 0.22f, 0.26f);
        glutSolidCube(1.0);
        glPopMatrix();

        // Rear Tie-Beams connecting to the Dome Corner Post
        float innerColX = xPos - side * 0.95f;
        float domePostX = side * 10.6f;
        float tieCenterX = (innerColX + domePostX) * 0.5f;
        float tieSpanX = fabsf(innerColX - domePostX);
        glColor3f(0.58f, 0.62f, 0.68f);
        glPushMatrix();
        glTranslatef(tieCenterX, 3.95f, towerZ);
        glScalef(tieSpanX, 0.20f, 0.20f);
        glutSolidCube(1.0);
        glPopMatrix();
        glPushMatrix();
        glTranslatef(tieCenterX, 5.85f, towerZ - 0.15f);
        glScalef(tieSpanX, 0.18f, 0.18f);
        glutSolidCube(1.0);
        glPopMatrix();

        // Front-Mounted 9x12 Live-Feed IMAG LED Screen (Unobstructed view)
        glPushMatrix();
        glTranslatef(xPos, screenY, screenZ);
        glRotatef(-side * 10.0f, 0.0f, 1.0f, 0.0f);

        drawLedCabinetPanel(sideW, wallH, 4, 3, liveFeedTextureID, 1.0f);

        // Top FEU Green Header & Blinking LIVE Tally Bar
        glPushMatrix();
        glTranslatef(0.0f, wallH * 0.5f + 0.26f, -0.02f);
        glScalef(sideW + 0.18f, 0.34f, 0.20f);
        glColor3f(0.04f, 0.36f, 0.18f);
        glutSolidCube(1.0);
        glPopMatrix();

        glDisable(GL_LIGHTING);
        glPushMatrix();
        glTranslatef(0.0f, wallH * 0.5f + 0.26f, 0.09f);
        glScalef(1.35f, 0.16f, 0.02f);
        bool blink = ((int)(simTime * 2.5f) % 2) == 0;
        if (blink)
            glColor3f(0.98f, 0.16f, 0.18f);
        else
            glColor3f(0.55f, 0.10f, 0.12f);
        glutSolidCube(1.0);
        glPopMatrix();
        glEnable(GL_LIGHTING);
        glPopMatrix();
    }
}

// ============================================================================
// STAGE DESIGNS: ARCHITECTURAL WINGS, TOTEMS, APRON CHEVRONS & BARRICADES
// ============================================================================
void drawStageDesigns(float stageY)
{
    // 1. Stage Apron Geometric Chevron Skirting Panels
    float chevronCenterY = stageY - 0.33f;
    for (int i = -4; i <= 4; i++)
    {
        float px = i * 2.1f;

        glPushMatrix();
        glTranslatef(px, chevronCenterY, 2.81f);
        glScalef(1.92f, 0.44f, 0.04f);
        glColor3f(0.09f, 0.10f, 0.12f);
        glutSolidCube(1.0);
        glPopMatrix();

        glDisable(GL_LIGHTING);
        for (int arm = -1; arm <= 1; arm += 2)
        {
            glPushMatrix();
            glTranslatef(px + arm * 0.36f, chevronCenterY, 2.84f);
            glRotatef(arm * 28.0f, 0.0f, 0.0f, 1.0f);
            glScalef(0.82f, 0.055f, 0.02f);
            if (abs(i) % 2 == 0)
                glColor3f(0.95f, 0.76f, 0.12f);
            else
                glColor3f(0.05f, 0.82f, 0.42f);
            glutSolidCube(1.0);
            glPopMatrix();
        }
        glEnable(GL_LIGHTING);
    }

    // 2. Center 9x24 LED Wall Truss Surround
    glColor3f(0.52f, 0.56f, 0.62f);
    for (int side = -1; side <= 1; side += 2)
    {
        glPushMatrix();
        glTranslatef(side * 4.42f, stageY + 1.78f, -4.15f);
        glScalef(0.24f, 3.55f, 0.24f);
        glutSolidCube(1.0);
        glPopMatrix();
    }
    glPushMatrix();
    glTranslatef(0.0f, stageY + 3.52f, -4.15f);
    glScalef(9.10f, 0.24f, 0.24f);
    glutSolidCube(1.0);
    glPopMatrix();

    // 3. Stage Left & Stage Right Decorative Geometric Wing Scrims
    for (int side = -1; side <= 1; side += 2)
    {
        float wx = side * 6.95f;
        float wz = -3.85f;

        glPushMatrix();
        glTranslatef(wx, stageY + 1.65f, wz);
        glRotatef(-side * 12.0f, 0.0f, 1.0f, 0.0f);

        glPushMatrix();
        glScalef(4.20f, 3.20f, 0.12f);
        glColor3f(0.05f, 0.22f, 0.13f);
        glutSolidCube(1.0);
        glPopMatrix();

        glPushMatrix();
        glTranslatef(0.0f, 1.62f, 0.02f);
        glScalef(4.30f, 0.10f, 0.16f);
        glColor3f(0.95f, 0.76f, 0.12f);
        glutSolidCube(1.0);
        glPopMatrix();

        glDisable(GL_LIGHTING);
        for (int louver = -2; louver <= 2; louver++)
        {
            glPushMatrix();
            glTranslatef(louver * 0.72f, 0.0f, 0.08f);
            glRotatef(-side * 18.0f, 0.0f, 0.0f, 1.0f);
            glScalef(0.10f, 2.75f, 0.03f);
            if (abs(louver) % 2 == 0)
                glColor3f(0.95f, 0.76f, 0.12f);
            else
                glColor3f(0.06f, 0.85f, 0.45f);
            glutSolidCube(1.0);
            glPopMatrix();
        }
        glEnable(GL_LIGHTING);
        glPopMatrix();
    }

    // 4. Four Upstage Vertical Light Totems
    float totemX[4] = {-7.6f, -4.9f, 4.9f, 7.6f};
    for (int t = 0; t < 4; t++)
    {
        glPushMatrix();
        glTranslatef(totemX[t], stageY, -3.35f);

        glPushMatrix();
        glTranslatef(0.0f, 0.05f, 0.0f);
        glScalef(0.65f, 0.10f, 0.65f);
        glColor3f(0.22f, 0.24f, 0.28f);
        glutSolidCube(1.0);
        glPopMatrix();

        glPushMatrix();
        glTranslatef(0.0f, 1.75f, 0.0f);
        glScalef(0.22f, 3.40f, 0.22f);
        glColor3f(0.55f, 0.58f, 0.64f);
        glutSolidCube(1.0);
        glPopMatrix();

        glDisable(GL_LIGHTING);
        float pulse = ledBarsOn ? (0.55f + 0.45f * sinf(simTime * 4.5f + t)) : 0.15f;
        glPushMatrix();
        glTranslatef(0.0f, 1.75f, 0.12f);
        glScalef(0.07f, 3.15f, 0.04f);
        if (t % 2 == 0)
            glColor3f(0.05f, 0.90f * pulse, 0.50f * pulse);
        else
            glColor3f(0.98f * pulse, 0.78f * pulse, 0.15f);
        glutSolidCube(1.0);
        glPopMatrix();

        glPushMatrix();
        glTranslatef(0.0f, 3.55f, 0.0f);
        glColor3f(0.98f, 0.82f, 0.20f);
        glutSolidSphere(0.14, 12, 12);
        glPopMatrix();
        glEnable(GL_LIGHTING);
        glPopMatrix();
    }

    // 5. Drum Riser Front Emblem Crest & Illuminated Side Steps
    glPushMatrix();
    glTranslatef(0.0f, stageY + 0.18f + riserHeight * 0.5f, -0.58f);
    glDisable(GL_LIGHTING);
    for (int arm = -1; arm <= 1; arm += 2)
    {
        glPushMatrix();
        glTranslatef(arm * 0.35f, 0.0f, 0.0f);
        glRotatef(arm * 24.0f, 0.0f, 0.0f, 1.0f);
        glScalef(0.78f, 0.07f, 0.03f);
        glColor3f(0.95f, 0.76f, 0.12f);
        glutSolidCube(1.0);
        glPopMatrix();
    }
    glEnable(GL_LIGHTING);
    glPopMatrix();

    for (int side = -1; side <= 1; side += 2)
    {
        glPushMatrix();
        glTranslatef(side * 2.25f, stageY + 0.09f, -1.85f);
        glScalef(0.65f, 0.18f, 1.6f);
        glColor3f(0.16f, 0.18f, 0.22f);
        glutSolidCube(1.0);
        glPopMatrix();
        glDisable(GL_LIGHTING);
        glPushMatrix();
        glTranslatef(side * 2.25f, stageY + 0.19f, -1.06f);
        glScalef(0.62f, 0.03f, 0.04f);
        glColor3f(0.95f, 0.76f, 0.12f);
        glutSolidCube(1.0);
        glPopMatrix();
        glEnable(GL_LIGHTING);
    }

    // 6. Front-of-Stage Crowd Barricade (Mojo Barrier)
    for (int b = -4; b <= 4; b++)
    {
        float bx = b * 2.1f;
        glPushMatrix();
        glTranslatef(bx, 0.42f, 4.65f);
        glScalef(2.02f, 0.84f, 0.08f);
        glColor3f(0.45f, 0.48f, 0.52f);
        glutSolidCube(1.0);
        glPopMatrix();

        glPushMatrix();
        glTranslatef(bx, 0.86f, 4.65f);
        glScalef(2.06f, 0.07f, 0.14f);
        glColor3f(0.72f, 0.75f, 0.80f);
        glutSolidCube(1.0);
        glPopMatrix();

        glPushMatrix();
        glTranslatef(bx, 0.03f, 4.45f);
        glScalef(2.00f, 0.05f, 0.55f);
        glColor3f(0.32f, 0.34f, 0.38f);
        glutSolidCube(1.0);
        glPopMatrix();
    }
}

// ============================================================================
// ATMOSPHERIC EFFECTS: LEFT & RIGHT STAGE HAZE MACHINES + CENTER-BOUND SMOKE
// ============================================================================
void drawHazeMachinesAndSmoke(float stageY)
{
    const float machineX = 8.85f;
    const float machineZ = -0.65f;

    for (int side = -1; side <= 1; side += 2)
    {
        glPushMatrix();
        // Position on Stage Left & Stage Right wings and orient front (+Z) inward toward center (x = 0)
        glTranslatef(side * machineX, stageY, machineZ);
        glRotatef(-side * 90.0f, 0.0f, 1.0f, 0.0f);

        // 1. Tour-Grade Road-Case Hazer Chassis
        glPushMatrix();
        glTranslatef(0.0f, 0.24f, 0.0f);
        glScalef(0.56f, 0.48f, 0.68f);
        glColor3f(0.11f, 0.12f, 0.14f);
        glutSolidCube(1.0);
        glPopMatrix();

        // 2. Brushed Aluminum Road-Case Trim & Corner Protectors
        glColor3f(0.66f, 0.69f, 0.74f);
        for (int edgeY = 0; edgeY <= 1; edgeY++)
        {
            float yEdge = (edgeY == 0) ? 0.02f : 0.47f;
            glPushMatrix();
            glTranslatef(0.0f, yEdge, 0.0f);
            glScalef(0.59f, 0.04f, 0.71f);
            glutSolidCube(1.0);
            glPopMatrix();
        }

        // Side Recessed Carrying Handles
        for (int hSide = -1; hSide <= 1; hSide += 2)
        {
            glPushMatrix();
            glTranslatef(hSide * 0.29f, 0.25f, 0.0f);
            glScalef(0.03f, 0.12f, 0.22f);
            glColor3f(0.32f, 0.34f, 0.38f);
            glutSolidCube(1.0);
            glPopMatrix();
        }

        // 3. Directional Output Nozzle & Angled Dispersion Louvers (Aimed inward & slightly up)
        glPushMatrix();
        glTranslatef(0.0f, 0.31f, 0.36f);
        glRotatef(-14.0f, 1.0f, 0.0f, 0.0f);

        glPushMatrix();
        glScalef(0.36f, 0.22f, 0.12f);
        glColor3f(0.18f, 0.19f, 0.22f);
        glutSolidCube(1.0);
        glPopMatrix();

        // Horizontal Directional Vanes
        glColor3f(0.54f, 0.57f, 0.62f);
        for (int vane = -1; vane <= 1; vane++)
        {
            glPushMatrix();
            glTranslatef(0.0f, vane * 0.055f, 0.05f);
            glScalef(0.31f, 0.018f, 0.05f);
            glutSolidCube(1.0);
            glPopMatrix();
        }
        glPopMatrix();

        // 4. Status Ready LED Indicator
        glDisable(GL_LIGHTING);
        glPushMatrix();
        glTranslatef(0.20f, 0.41f, 0.35f);
        if (hazeMachinesOn)
            glColor3f(0.08f, 0.96f, 0.46f);
        else
            glColor3f(0.45f, 0.10f, 0.10f);
        glutSolidSphere(0.024, 8, 8);
        glPopMatrix();

        // 5. Volumetric Haze Plume Flowing Directly Toward Stage Center (local +Z)
        if (hazeMachinesOn)
        {
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            glDepthMask(GL_FALSE);

            const int numPuffs = 20;
            for (int i = 0; i < numPuffs; i++)
            {
                float t = fmodf(simTime * 0.38f + (float)i / (float)numPuffs, 1.0f);

                // Travel distance from nozzle (0.42f) to exact stage center (machineX = 8.85f)
                float distToCenter = 0.42f + (machineX - 0.35f) * t;

                // Organic cross-stage swirl & gentle upward dispersion
                float swirlX = 0.52f * sinf(t * 4.4f + i * 0.75f + side * 1.1f) * t;
                float riseY = 0.33f + 1.38f * powf(t, 0.90f) + 0.14f * cosf(simTime * 2.1f + i * 0.9f);

                // Smooth expansion & soft edge fade so smoke blends seamlessly at center stage
                float radius = 0.13f + 0.78f * powf(t, 0.85f);
                float envelope = sinf(t * (float)M_PI);
                float alpha = 0.125f * envelope * (1.0f - 0.30f * t);

                glPushMatrix();
                glTranslatef(swirlX, riseY, distToCenter);
                glScalef(radius * 1.25f, radius * 0.85f, radius * 1.55f);
                glColor4f(0.76f, 0.90f, 0.92f, alpha);
                glutSolidSphere(1.0, 12, 10);
                glPopMatrix();
            }

            glDepthMask(GL_TRUE);
            glDisable(GL_BLEND);
        }
        glEnable(GL_LIGHTING);
        glPopMatrix();
    }
}

// ============================================================================
// SPEAKERS: FLOWN LINE ARRAYS (8 BOXES PER SIDE) & 4 FLOOR SUBWOOFERS
// ============================================================================
void drawLineArrayHang(float xPos, float topY, float zPos, float yawInwardDeg)
{
    const int numBoxes = 8;

    glPushMatrix();
    glTranslatef(xPos, topY, zPos);
    glRotatef(yawInwardDeg, 0.0f, 1.0f, 0.0f);

    glDisable(GL_LIGHTING);
    glColor3f(0.75f, 0.78f, 0.82f);
    glLineWidth(2.0f);
    glBegin(GL_LINES);
    glVertex3f(-0.42f, 0.0f, -0.15f);
    glVertex3f(-0.42f, 0.55f, -0.15f);
    glVertex3f(0.42f, 0.0f, -0.15f);
    glVertex3f(0.42f, 0.55f, -0.15f);
    glEnd();
    glEnable(GL_LIGHTING);

    glPushMatrix();
    glScalef(1.08f, 0.10f, 0.68f);
    glColor3f(0.28f, 0.30f, 0.34f);
    glutSolidCube(1.0);
    glPopMatrix();

    float curY = -0.22f;
    float curZ = 0.0f;
    for (int b = 0; b < numBoxes; b++)
    {
        float tiltAngle = 2.0f + b * 3.4f;

        glPushMatrix();
        glTranslatef(0.0f, curY, curZ);
        glRotatef(tiltAngle, 1.0f, 0.0f, 0.0f);

        glPushMatrix();
        glScalef(0.98f, 0.27f, 0.56f);
        glColor3f(0.11f, 0.11f, 0.14f);
        glutSolidCube(1.0);
        glPopMatrix();

        glPushMatrix();
        glTranslatef(0.0f, 0.0f, 0.285f);
        glScalef(0.88f, 0.22f, 0.02f);
        glColor3f(0.22f, 0.23f, 0.26f);
        glutSolidCube(1.0);
        glPopMatrix();

        for (int side = -1; side <= 1; side += 2)
        {
            glPushMatrix();
            glTranslatef(side * 0.26f, 0.0f, 0.295f);
            glScalef(bassPulse, bassPulse, 0.4f);
            glColor3f(0.06f, 0.06f, 0.08f);
            glutSolidSphere(0.085, 10, 10);
            glPopMatrix();
        }

        glColor3f(0.62f, 0.65f, 0.70f);
        for (int side = -1; side <= 1; side += 2)
        {
            glPushMatrix();
            glTranslatef(side * 0.50f, 0.0f, 0.0f);
            glScalef(0.03f, 0.29f, 0.42f);
            glutSolidCube(1.0);
            glPopMatrix();
        }
        glPopMatrix();

        float rad = tiltAngle * (float)M_PI / 180.0f;
        curY -= 0.30f * cosf(rad);
        curZ -= 0.14f * sinf(rad);
    }
    glPopMatrix();
}

void drawFloorSubwoofer(float xPos, float zPos)
{
    glPushMatrix();
    glTranslatef(xPos, 0.0f, zPos);

    glPushMatrix();
    glTranslatef(0.0f, 0.54f, 0.0f);
    glScalef(1.35f, 1.08f, 0.92f);
    glColor3f(0.10f, 0.10f, 0.13f);
    glutSolidCube(1.0);
    glPopMatrix();

    glPushMatrix();
    glTranslatef(0.0f, 1.10f, 0.0f);
    glScalef(1.37f, 0.04f, 0.94f);
    glColor3f(0.22f, 0.22f, 0.26f);
    glutSolidCube(1.0);
    glPopMatrix();

    float driverX[2] = {-0.34f, 0.34f};
    for (int i = 0; i < 2; i++)
    {
        glPushMatrix();
        glTranslatef(driverX[i], 0.62f, 0.47f);
        glColor3f(0.45f, 0.46f, 0.52f);
        glutSolidTorus(0.04, 0.27, 12, 22);

        glScalef(bassPulse, bassPulse, 0.45f * bassPulse);
        glColor3f(0.06f, 0.06f, 0.08f);
        glutSolidSphere(0.25, 16, 16);
        glPopMatrix();
    }
    glPopMatrix();
}

// ============================================================================
// LIGHTING FIXTURES: ARTICULATED MOVING HEADS, STROBES, PARS, BARS & STORMY
// ============================================================================
void drawMovingHeadFixture(float x, float y, float z,
                           float panDeg, float tiltDeg,
                           float r, float g, float b,
                           bool isOn, bool floorMounted = false,
                           float beamRadius = 2.0f, float beamLength = 7.4f)
{
    glPushMatrix();
    glTranslatef(x, y, z);

    // Flip upright when mounted on the stage floor instead of hanging from a truss
    if (floorMounted)
    {
        glRotatef(180.0f, 0.0f, 0.0f, 1.0f);
    }

    // 1. Truss Clamp / Floor Mounting Base Housing
    glPushMatrix();
    glTranslatef(0.0f, -0.05f, 0.0f);
    glScalef(0.34f, 0.10f, 0.26f);
    glColor3f(0.14f, 0.14f, 0.17f);
    glutSolidCube(1.0);
    glPopMatrix();

    // 2. Pan-Rotating U-Shaped Yoke Arm
    glRotatef(panDeg, 0.0f, 1.0f, 0.0f);

    glColor3f(0.18f, 0.19f, 0.23f);
    glPushMatrix();
    glTranslatef(0.0f, -0.13f, 0.0f);
    glScalef(0.44f, 0.06f, 0.10f);
    glutSolidCube(1.0);
    glPopMatrix();
    for (int side = -1; side <= 1; side += 2)
    {
        glPushMatrix();
        glTranslatef(side * 0.20f, -0.26f, 0.0f);
        glScalef(0.05f, 0.28f, 0.10f);
        glutSolidCube(1.0);
        glPopMatrix();
    }

    // 3. Tilt-Rotating Motorized Optical Head
    glTranslatef(0.0f, -0.34f, 0.0f);
    glRotatef(tiltDeg, 1.0f, 0.0f, 0.0f);

    glPushMatrix();
    glTranslatef(0.0f, 0.14f, 0.0f);
    glRotatef(90.0f, 1.0f, 0.0f, 0.0f);
    glColor3f(0.12f, 0.12f, 0.15f);
    drawCylinder(0.14f, 0.17f, 0.38f, 16);
    glPopMatrix();

    // Glowing Front Lens Element
    bool partyBlink = (lightShowMode != 2) || (((int)(simTime * 10.0f + fabsf(x) * 3.0f) % 2) == 0);
    bool activeBeam = isOn && partyBlink;

    glDisable(GL_LIGHTING);
    glPushMatrix();
    glTranslatef(0.0f, -0.245f, 0.0f);
    glRotatef(90.0f, 1.0f, 0.0f, 0.0f);
    if (activeBeam)
        glColor3f(r, g, b);
    else
        glColor3f(0.18f, 0.20f, 0.24f);
    drawCylinder(0.15f, 0.15f, 0.01f, 16);
    glPopMatrix();

    // 4. Volumetric Light Beam Cone
    if (activeBeam)
    {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDepthMask(GL_FALSE);

        glPushMatrix();
        glTranslatef(0.0f, -0.25f - beamLength, 0.0f);
        glRotatef(-90.0f, 1.0f, 0.0f, 0.0f);
        glColor4f(r, g, b, 0.13f);
        glutSolidCone(beamRadius, beamLength, 22, 1);
        glPopMatrix();

        glDepthMask(GL_TRUE);
        glDisable(GL_BLEND);
    }
    glEnable(GL_LIGHTING);
    glPopMatrix();
}

void drawStrobeFixture(float x, float y, float z, float tiltDeg)
{
    bool flash = isStrobeFlashing();
    glPushMatrix();
    glTranslatef(x, y, z);
    glRotatef(tiltDeg, 1.0f, 0.0f, 0.0f);

    glPushMatrix();
    glScalef(0.72f, 0.30f, 0.28f);
    glColor3f(0.14f, 0.14f, 0.16f);
    glutSolidCube(1.0);
    glPopMatrix();

    glDisable(GL_LIGHTING);
    glPushMatrix();
    glTranslatef(0.0f, 0.0f, 0.15f);
    glScalef(0.62f, 0.20f, 0.03f);
    if (flash)
        glColor3f(1.0f, 1.0f, 1.0f);
    else
        glColor3f(0.28f, 0.30f, 0.35f);
    glutSolidCube(1.0);
    glPopMatrix();

    if (flash)
    {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDepthMask(GL_FALSE);
        glPushMatrix();
        glTranslatef(0.0f, -5.6f, 1.5f);
        glRotatef(-75.0f, 1.0f, 0.0f, 0.0f);
        glColor4f(1.0f, 1.0f, 1.0f, 0.22f);
        glutSolidCone(3.2, 6.0, 18, 1);
        glPopMatrix();
        glDepthMask(GL_TRUE);
        glDisable(GL_BLEND);
    }
    glEnable(GL_LIGHTING);
    glPopMatrix();
}

void drawLedParFixture(float x, float y, float z, float yawDeg, float pitchDeg, int colorIndex)
{
    float r = 0.5f + 0.5f * sinf(simTime * 2.2f + colorIndex * 1.1f);
    float g = 0.5f + 0.5f * sinf(simTime * 2.2f + colorIndex * 1.1f + 2.09f);
    float b = 0.5f + 0.5f * sinf(simTime * 2.2f + colorIndex * 1.1f + 4.18f);

    glPushMatrix();
    glTranslatef(x, y, z);
    glRotatef(yawDeg, 0.0f, 0.0f, 1.0f);
    glRotatef(pitchDeg, 1.0f, 0.0f, 0.0f);

    glColor3f(0.12f, 0.12f, 0.14f);
    drawCylinder(0.20f, 0.23f, 0.38f, 14);

    glDisable(GL_LIGHTING);
    glPushMatrix();
    glTranslatef(0.0f, 0.0f, 0.39f);
    if (ledParsOn)
        glColor3f(r, g, b);
    else
        glColor3f(0.18f, 0.18f, 0.22f);
    glutSolidSphere(0.19, 12, 12);
    glPopMatrix();

    if (ledParsOn)
    {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDepthMask(GL_FALSE);
        glPushMatrix();
        glTranslatef(0.0f, 0.0f, 0.39f);
        glColor4f(r, g, b, 0.10f);
        glutSolidCone(1.85, 5.4, 18, 1);
        glPopMatrix();
        glDepthMask(GL_TRUE);
        glDisable(GL_BLEND);
    }
    glEnable(GL_LIGHTING);
    glPopMatrix();
}

void drawLedPixelBar(float x, float y, float z, float rotZDeg, float length)
{
    const int cells = 8;
    float cellW = length / cells;

    glPushMatrix();
    glTranslatef(x, y, z);
    glRotatef(rotZDeg, 0.0f, 0.0f, 1.0f);

    glPushMatrix();
    glScalef(length, 0.15f, 0.15f);
    glColor3f(0.12f, 0.12f, 0.15f);
    glutSolidCube(1.0);
    glPopMatrix();

    glDisable(GL_LIGHTING);
    for (int i = 0; i < cells; i++)
    {
        float offset = -length * 0.5f + cellW * (i + 0.5f);
        float phase = simTime * 6.0f - i * 0.65f;
        float active = ledBarsOn ? (0.35f + 0.65f * fabsf(sinf(phase))) : 0.08f;

        float r = active * (0.5f + 0.5f * sinf(i * 0.5f + simTime));
        float g = active * (0.5f + 0.5f * cosf(i * 0.5f - simTime));
        float b = active * 1.0f;

        glPushMatrix();
        glTranslatef(offset, 0.0f, 0.08f);
        glScalef(cellW * 0.82f, 0.11f, 0.02f);
        glColor3f(r, g, b);
        glutSolidCube(1.0);
        glPopMatrix();
    }
    glEnable(GL_LIGHTING);
    glPopMatrix();
}

void drawStormyLightFixture(float x, float y, float z, float yawDeg)
{
    bool stormFlash = isStormyFlashing();
    glPushMatrix();
    glTranslatef(x, y, z);
    glRotatef(yawDeg, 0.0f, 1.0f, 0.0f);
    glRotatef(25.0f, 1.0f, 0.0f, 0.0f);

    glPushMatrix();
    glScalef(0.82f, 0.46f, 0.32f);
    glColor3f(0.18f, 0.19f, 0.22f);
    glutSolidCube(1.0);
    glPopMatrix();

    glDisable(GL_LIGHTING);
    float cx[2] = {-0.20f, 0.20f};
    float cy[2] = {-0.10f, 0.10f};
    for (int ix = 0; ix < 2; ix++)
    {
        for (int iy = 0; iy < 2; iy++)
        {
            glPushMatrix();
            glTranslatef(cx[ix], cy[iy], 0.17f);
            if (stormFlash)
            {
                glColor3f(0.85f, 0.96f, 1.0f);
            }
            else if (stormyOn)
            {
                glColor3f(0.18f, 0.35f, 0.55f);
            }
            else
            {
                glColor3f(0.12f, 0.12f, 0.15f);
            }
            glutSolidSphere(0.08, 10, 10);
            glPopMatrix();
        }
    }

    if (stormFlash)
    {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDepthMask(GL_FALSE);
        glPushMatrix();
        glTranslatef(0.0f, 0.0f, 0.19f);
        glColor4f(0.75f, 0.93f, 1.0f, 0.24f);
        glutSolidCone(3.4, 7.5, 20, 1);
        glPopMatrix();
        glDepthMask(GL_TRUE);
        glDisable(GL_BLEND);
    }
    glEnable(GL_LIGHTING);
    glPopMatrix();
}

// ============================================================================
// INSTRUMENTS & STAGE GEAR (Drums, Held Guitars, Keyboard, Mic Stands)
// ============================================================================
void drawDrumKit(float x, float y, float z)
{
    glPushMatrix();
    glTranslatef(x, y, z);

    glPushMatrix();
    glTranslatef(0.0f, 0.44f, 0.0f);
    glScalef(bassPulse, bassPulse, 1.0f);
    glColor3f(0.04f, 0.38f, 0.18f);
    drawCylinder(0.44f, 0.44f, 0.50f, 24);

    glTranslatef(0.0f, 0.0f, 0.505f);
    glColor3f(0.95f, 0.92f, 0.84f);
    drawCylinder(0.42f, 0.42f, 0.01f, 24);
    glPopMatrix();

    glPushMatrix();
    glTranslatef(-0.46f, 0.54f, -0.18f);
    glRotatef(-90.0f, 1.0f, 0.0f, 0.0f);
    glColor3f(0.85f, 0.72f, 0.20f);
    drawCylinder(0.21f, 0.21f, 0.15f, 18);
    glPopMatrix();
    glPushMatrix();
    glTranslatef(-0.46f, 0.27f, -0.18f);
    glScalef(0.04f, 0.54f, 0.04f);
    glColor3f(0.65f, 0.65f, 0.70f);
    glutSolidCube(1.0);
    glPopMatrix();

    float tomX[2] = {-0.19f, 0.19f};
    for (int i = 0; i < 2; i++)
    {
        glPushMatrix();
        glTranslatef(tomX[i], 0.90f, 0.18f);
        glRotatef(-68.0f, 1.0f, 0.0f, 0.0f);
        glColor3f(0.04f, 0.38f, 0.18f);
        drawCylinder(0.17f, 0.17f, 0.17f, 16);
        glPopMatrix();
    }

    glPushMatrix();
    glTranslatef(0.52f, 0.35f, -0.15f);
    glRotatef(-90.0f, 1.0f, 0.0f, 0.0f);
    glColor3f(0.04f, 0.38f, 0.18f);
    drawCylinder(0.25f, 0.25f, 0.35f, 18);
    glPopMatrix();

    float cymbalSway = bandJamActive ? (4.5f * sinf(simTime * 9.0f)) : 0.0f;
    float cymPos[3][3] = {
        {-0.74f, 0.85f, -0.02f},
        {-0.56f, 1.24f, 0.18f},
        {0.64f, 1.28f, 0.14f}};
    for (int c = 0; c < 3; c++)
    {
        glPushMatrix();
        glTranslatef(cymPos[c][0], cymPos[c][1] * 0.5f, cymPos[c][2]);
        glScalef(0.03f, cymPos[c][1], 0.03f);
        glColor3f(0.7f, 0.7f, 0.75f);
        glutSolidCube(1.0);
        glPopMatrix();

        glPushMatrix();
        glTranslatef(cymPos[c][0], cymPos[c][1], cymPos[c][2]);
        glRotatef(cymbalSway * (c == 0 ? 0.3f : 1.0f), 0.0f, 0.0f, 1.0f);
        glRotatef(-90.0f, 1.0f, 0.0f, 0.0f);
        glColor3f(0.92f, 0.76f, 0.22f);
        glutSolidCone(c == 0 ? 0.23 : 0.30, 0.03, 18, 1);
        glPopMatrix();
    }
    glPopMatrix();
}

void drawStrappedGuitar(bool isBass, float r, float g, float b)
{
    glPushMatrix();
    glRotatef(26.0f, 0.0f, 0.0f, 1.0f);

    glPushMatrix();
    glTranslatef(-0.04f, 0.16f, -0.10f);
    glRotatef(-36.0f, 0.0f, 0.0f, 1.0f);
    glScalef(0.05f, 0.68f, 0.24f);
    glColor3f(0.10f, 0.10f, 0.12f);
    glutSolidCube(1.0);
    glPopMatrix();

    glColor3f(r, g, b);
    glPushMatrix();
    glTranslatef(-0.14f, 0.0f, 0.0f);
    glScalef(0.46f, 0.38f, 0.07f);
    glutSolidSphere(0.55, 16, 16);
    glPopMatrix();
    glPushMatrix();
    glTranslatef(0.10f, 0.02f, 0.0f);
    glScalef(0.36f, 0.30f, 0.07f);
    glutSolidSphere(0.55, 16, 16);
    glPopMatrix();

    glPushMatrix();
    glTranslatef(-0.04f, -0.02f, 0.04f);
    glScalef(0.26f, 0.18f, 0.01f);
    glColor3f(0.95f, 0.95f, 0.95f);
    glutSolidCube(1.0);
    glPopMatrix();

    float neckLen = isBass ? 0.84f : 0.68f;
    glPushMatrix();
    glTranslatef(0.24f + neckLen * 0.5f, 0.02f, 0.0f);
    glScalef(neckLen, 0.065f, 0.035f);
    glColor3f(0.76f, 0.58f, 0.36f);
    glutSolidCube(1.0);
    glPopMatrix();

    glPushMatrix();
    glTranslatef(0.24f + neckLen + 0.08f, 0.03f, 0.0f);
    glScalef(0.16f, 0.10f, 0.035f);
    glColor3f(r, g, b);
    glutSolidCube(1.0);
    glPopMatrix();
    glPopMatrix();
}

void drawKeyboardRig(float x, float y, float z, float yawDeg)
{
    glPushMatrix();
    glTranslatef(x, y, z);
    glRotatef(yawDeg, 0.0f, 1.0f, 0.0f);

    glColor3f(0.22f, 0.22f, 0.26f);
    for (int side = -1; side <= 1; side += 2)
    {
        glPushMatrix();
        glTranslatef(side * 0.48f, 0.44f, 0.0f);
        glRotatef(35.0f, 1.0f, 0.0f, 0.0f);
        glScalef(0.05f, 1.02f, 0.05f);
        glutSolidCube(1.0);
        glPopMatrix();

        glPushMatrix();
        glTranslatef(side * 0.48f, 0.44f, 0.0f);
        glRotatef(-35.0f, 1.0f, 0.0f, 0.0f);
        glScalef(0.05f, 1.02f, 0.05f);
        glutSolidCube(1.0);
        glPopMatrix();
    }

    glPushMatrix();
    glTranslatef(0.0f, 0.88f, 0.0f);
    glScalef(1.45f, 0.11f, 0.44f);
    glColor3f(0.14f, 0.14f, 0.18f);
    glutSolidCube(1.0);
    glPopMatrix();

    glPushMatrix();
    glTranslatef(0.0f, 0.91f, -0.12f);
    glScalef(1.30f, 0.06f, 0.18f);
    glColor3f(0.95f, 0.95f, 0.97f);
    glutSolidCube(1.0);
    glPopMatrix();
    glPopMatrix();
}

void drawMicStandAndMonitor(float x, float y, float z, bool withMonitor)
{
    glPushMatrix();
    glTranslatef(x, y, z);

    glColor3f(0.16f, 0.16f, 0.18f);
    glPushMatrix();
    glRotatef(-90.0f, 1.0f, 0.0f, 0.0f);
    drawCylinder(0.19f, 0.17f, 0.04f, 16);
    glPopMatrix();

    glPushMatrix();
    glTranslatef(0.0f, 0.68f, 0.0f);
    glScalef(0.025f, 1.36f, 0.025f);
    glColor3f(0.72f, 0.74f, 0.78f);
    glutSolidCube(1.0);
    glPopMatrix();

    glPushMatrix();
    glTranslatef(0.0f, 1.36f, -0.05f);
    glRotatef(-18.0f, 1.0f, 0.0f, 0.0f);
    glPushMatrix();
    glScalef(0.02f, 0.02f, 0.42f);
    glutSolidCube(1.0);
    glPopMatrix();
    glTranslatef(0.0f, 0.0f, -0.22f);
    glColor3f(0.85f, 0.86f, 0.90f);
    glutSolidSphere(0.048, 12, 12);
    glPopMatrix();

    if (withMonitor)
    {
        glPushMatrix();
        glTranslatef(0.0f, 0.17f, 0.48f);
        glRotatef(-32.0f, 1.0f, 0.0f, 0.0f);
        glScalef(0.58f, 0.30f, 0.38f);
        glColor3f(0.12f, 0.12f, 0.15f);
        glutSolidCube(1.0);
        glPopMatrix();
    }
    glPopMatrix();
}

// ============================================================================
// PERFORMERS: ANIMATED 3D BAND MEMBERS & LEAD SINGER
// ============================================================================
void drawArticulatedArm(float shoulderX, float shoulderY, float shoulderZ,
                        float pitchDeg, float yawDeg, float rollDeg,
                        float elbowBendDeg,
                        float sleeveR, float sleeveG, float sleeveB,
                        bool hasDrumstick)
{
    glPushMatrix();
    glTranslatef(shoulderX, shoulderY, shoulderZ);
    glRotatef(yawDeg, 0.0f, 1.0f, 0.0f);
    glRotatef(rollDeg, 0.0f, 0.0f, 1.0f);
    glRotatef(pitchDeg, 1.0f, 0.0f, 0.0f);

    glColor3f(sleeveR, sleeveG, sleeveB);
    glutSolidSphere(0.065, 10, 10);

    glPushMatrix();
    glRotatef(90.0f, 1.0f, 0.0f, 0.0f);
    drawCylinder(0.055f, 0.048f, 0.28f, 10);
    glPopMatrix();

    glTranslatef(0.0f, -0.28f, 0.0f);
    glRotatef(elbowBendDeg, 1.0f, 0.0f, 0.0f);

    glColor3f(0.88f, 0.72f, 0.58f);
    glutSolidSphere(0.048, 10, 10);
    glPushMatrix();
    glRotatef(90.0f, 1.0f, 0.0f, 0.0f);
    drawCylinder(0.045f, 0.038f, 0.26f, 10);
    glPopMatrix();

    glTranslatef(0.0f, -0.27f, 0.0f);
    glutSolidSphere(0.045, 10, 10);

    if (hasDrumstick)
    {
        glPushMatrix();
        glRotatef(-75.0f, 1.0f, 0.0f, 0.0f);
        glColor3f(0.86f, 0.72f, 0.46f);
        drawCylinder(0.012f, 0.007f, 0.34f, 8);
        glPopMatrix();
    }
    glPopMatrix();
}

void drawMusician(int role, float x, float y, float z, float yawDeg,
                  float outfitR, float outfitG, float outfitB)
{
    float beat = bandJamActive ? sinf(simTime * 7.5f + role * 1.1f) : 0.0f;
    float bobY = bandJamActive ? (0.025f * fabsf(beat)) : 0.0f;
    float headNod = bandJamActive ? (8.0f * beat) : 0.0f;

    glPushMatrix();
    glTranslatef(x, y, z);
    glRotatef(yawDeg, 0.0f, 1.0f, 0.0f);

    if (role == 1)
    {
        // Drummer's Padded Throne Stool
        glPushMatrix();
        glTranslatef(0.0f, 0.24f, -0.05f);
        glScalef(0.06f, 0.48f, 0.06f);
        glColor3f(0.55f, 0.55f, 0.60f);
        glutSolidCube(1.0);
        glPopMatrix();
        glPushMatrix();
        glTranslatef(0.0f, 0.48f, -0.05f);
        glRotatef(-90.0f, 1.0f, 0.0f, 0.0f);
        glColor3f(0.12f, 0.12f, 0.14f);
        drawCylinder(0.18f, 0.18f, 0.07f, 16);
        glPopMatrix();

        float legX[2] = {-0.14f, 0.14f};
        for (int i = 0; i < 2; i++)
        {
            glPushMatrix();
            glTranslatef(legX[i], 0.52f, 0.02f);
            glRotatef(i == 0 ? -14.0f : 14.0f, 0.0f, 1.0f, 0.0f);
            glColor3f(0.12f, 0.14f, 0.20f);
            drawCylinder(0.075f, 0.065f, 0.34f, 10);

            glTranslatef(0.0f, 0.0f, 0.34f);
            glRotatef(82.0f, 1.0f, 0.0f, 0.0f);
            drawCylinder(0.060f, 0.050f, 0.44f, 10);

            glTranslatef(0.0f, 0.04f, 0.44f);
            glScalef(0.11f, 0.18f, 0.08f);
            glColor3f(0.92f, 0.92f, 0.95f);
            glutSolidCube(1.0);
            glPopMatrix();
        }
        glTranslatef(0.0f, -0.26f + bobY * 0.6f, -0.04f);
    }
    else
    {
        float legX[2] = {-0.11f, 0.11f};
        for (int i = 0; i < 2; i++)
        {
            glPushMatrix();
            glTranslatef(legX[i], 0.045f, 0.04f);
            glScalef(0.11f, 0.09f, 0.22f);
            glColor3f(0.92f, 0.92f, 0.95f);
            glutSolidCube(1.0);
            glPopMatrix();

            glPushMatrix();
            glTranslatef(legX[i], 0.08f, 0.0f);
            glRotatef(-90.0f, 1.0f, 0.0f, 0.0f);
            glColor3f(0.12f, 0.14f, 0.20f);
            drawCylinder(0.062f, 0.075f, 0.76f - bobY, 12);
            glPopMatrix();
        }
        glTranslatef(0.0f, -bobY, 0.0f);
    }

    float torsoSway = bandJamActive ? (4.5f * sinf(simTime * 3.8f + role)) : 0.0f;
    glPushMatrix();
    glTranslatef(0.0f, 0.82f, 0.0f);
    glRotatef(torsoSway, 0.0f, 1.0f, 0.0f);
    if (role == 1)
        glRotatef(10.0f, 1.0f, 0.0f, 0.0f);

    glPushMatrix();
    glRotatef(-90.0f, 1.0f, 0.0f, 0.0f);
    glColor3f(outfitR, outfitG, outfitB);
    drawCylinder(0.16f, 0.19f, 0.48f, 14);
    glPopMatrix();

    glPushMatrix();
    glTranslatef(0.0f, 0.24f, 0.165f);
    glScalef(0.06f, 0.44f, 0.03f);
    glColor3f(0.95f, 0.76f, 0.12f);
    glutSolidCube(1.0);
    glPopMatrix();

    glPushMatrix();
    glTranslatef(0.0f, 0.52f, 0.0f);
    glRotatef(headNod, 1.0f, 0.0f, 0.0f);

    glColor3f(0.88f, 0.72f, 0.58f);
    glPushMatrix();
    glRotatef(-90.0f, 1.0f, 0.0f, 0.0f);
    drawCylinder(0.055f, 0.055f, 0.09f, 10);
    glPopMatrix();

    glTranslatef(0.0f, 0.16f, 0.01f);
    glutSolidSphere(0.135, 16, 16);

    glPushMatrix();
    glTranslatef(0.0f, 0.035f, -0.015f);
    glScalef(1.04f, 1.02f, 1.06f);
    glColor3f(0.10f, 0.09f, 0.08f);
    glutSolidSphere(0.136, 14, 14);
    glPopMatrix();

    if (role == 0)
    {
        glPushMatrix();
        glTranslatef(0.0f, 0.01f, 0.128f);
        glScalef(0.17f, 0.042f, 0.03f);
        glColor3f(0.05f, 0.05f, 0.08f);
        glutSolidCube(1.0);
        glPopMatrix();
    }
    glPopMatrix();

    if (role == 0)
    {
        float waveArm = bandJamActive ? (-115.0f + 18.0f * sinf(simTime * 5.0f)) : -45.0f;
        drawArticulatedArm(-0.21f, 0.44f, 0.0f,
                           -58.0f, 18.0f, 0.0f, -42.0f,
                           outfitR, outfitG, outfitB, false);
        drawArticulatedArm(0.21f, 0.44f, 0.0f,
                           waveArm, -15.0f, -18.0f, -35.0f,
                           outfitR, outfitG, outfitB, false);
    }
    else if (role == 1)
    {
        float hitR = bandJamActive ? (-52.0f + 26.0f * sinf(simTime * 15.0f)) : -45.0f;
        float hitL = bandJamActive ? (-52.0f + 26.0f * cosf(simTime * 15.0f)) : -45.0f;
        drawArticulatedArm(-0.21f, 0.44f, 0.0f,
                           hitR, 12.0f, 0.0f, -55.0f,
                           outfitR, outfitG, outfitB, true);
        drawArticulatedArm(0.21f, 0.44f, 0.0f,
                           hitL, -14.0f, 0.0f, -55.0f,
                           outfitR, outfitG, outfitB, true);
    }
    else if (role == 2 || role == 3)
    {
        bool isBass = (role == 3);
        glPushMatrix();
        glTranslatef(0.05f, 0.14f, 0.19f);
        if (isBass)
            drawStrappedGuitar(true, 0.95f, 0.74f, 0.14f);
        else
            drawStrappedGuitar(false, 0.04f, 0.68f, 0.35f);
        glPopMatrix();

        float strum = bandJamActive ? (-48.0f + 14.0f * sinf(simTime * (isBass ? 8.0f : 13.0f))) : -45.0f;
        float fret = bandJamActive ? (-64.0f + 6.0f * sinf(simTime * 4.5f)) : -62.0f;

        drawArticulatedArm(-0.21f, 0.44f, 0.0f,
                           strum, 24.0f, 0.0f, -68.0f,
                           outfitR, outfitG, outfitB, false);
        drawArticulatedArm(0.21f, 0.44f, 0.0f,
                           fret, 32.0f, -22.0f, -42.0f,
                           outfitR, outfitG, outfitB, false);
    }
    else if (role == 4)
    {
        float playR = bandJamActive ? (-52.0f + 8.0f * sinf(simTime * 11.0f)) : -50.0f;
        float playL = bandJamActive ? (-52.0f + 8.0f * cosf(simTime * 11.0f)) : -50.0f;

        drawArticulatedArm(-0.21f, 0.44f, 0.0f,
                           playR, 8.0f, 0.0f, -58.0f,
                           outfitR, outfitG, outfitB, false);
        drawArticulatedArm(0.21f, 0.44f, 0.0f,
                           playL, -8.0f, 0.0f, -58.0f,
                           outfitR, outfitG, outfitB, false);
    }
    glPopMatrix();
    glPopMatrix();
}

// ============================================================================
// ANIMATED CONCERT AUDIENCE (linked to the band's B-key toggle)
// ============================================================================
void drawAudiencePerson(float x, float z, int id)
{
    const float phase = id * 1.37f;
    const float beat = bandJamActive ? sinf(simTime * 7.5f + phase) : 0.0f;
    const float jump = bandJamActive ? 0.12f * fabsf(beat) : 0.0f;
    const float sway = bandJamActive ? 9.0f * sinf(simTime * 3.8f + phase) : 0.0f;
    const float armWave = bandJamActive ? 35.0f * sinf(simTime * 6.0f + phase) : 0.0f;

    const float colors[6][3] = {
        {0.05f, 0.45f, 0.22f}, {0.95f, 0.75f, 0.12f},
        {0.15f, 0.22f, 0.65f}, {0.70f, 0.18f, 0.30f},
        {0.85f, 0.85f, 0.88f}, {0.18f, 0.18f, 0.22f}
    };
    const float *shirt = colors[id % 6];

    glPushMatrix();
    glTranslatef(x, jump, z);
    // The stage faces toward negative Z, so the crowd faces positive Z.
    glRotatef(sway, 0.0f, 1.0f, 0.0f);

    // Legs
    glColor3f(0.12f, 0.14f, 0.20f);
    for (int side = -1; side <= 1; side += 2)
    {
        glPushMatrix();
        glTranslatef(side * 0.12f, 0.38f, 0.0f);
        glScalef(0.16f, 0.76f, 0.18f);
        glutSolidCube(1.0);
        glPopMatrix();
    }

    // Shirt / torso
    glColor3f(shirt[0], shirt[1], shirt[2]);
    glPushMatrix();
    glTranslatef(0.0f, 1.08f, 0.0f);
    glScalef(0.46f, 0.72f, 0.26f);
    glutSolidCube(1.0);
    glPopMatrix();

    // Head and hair
    glColor3f(0.82f, 0.62f, 0.46f);
    glPushMatrix();
    glTranslatef(0.0f, 1.62f, 0.0f);
    glutSolidSphere(0.20, 10, 8);
    glTranslatef(0.0f, 0.12f, 0.0f);
    glScalef(1.02f, 0.45f, 1.02f);
    glColor3f(0.10f, 0.09f, 0.08f);
    glutSolidSphere(0.20, 10, 8);
    glPopMatrix();

    // Arms raised and waving while the band performs; relaxed when stopped.
    for (int side = -1; side <= 1; side += 2)
    {
        glPushMatrix();
        glTranslatef(side * 0.29f, 1.37f, 0.0f);
        const float angle = bandJamActive
            ? side * (125.0f + armWave) : side * 12.0f;
        glRotatef(angle, 0.0f, 0.0f, 1.0f);
        glColor3f(0.82f, 0.62f, 0.46f);
        glTranslatef(0.0f, -0.30f, 0.0f);
        glScalef(0.12f, 0.60f, 0.12f);
        glutSolidCube(1.0);
        glPopMatrix();
    }
    glPopMatrix();
}

void drawConcertCrowd()
{
    const int rows = 7;
    const int columns = 22;
    // World-space audience, in front of the barricade and on the field.
    for (int row = 0; row < rows; ++row)
    {
        for (int col = 0; col < columns; ++col)
        {
            const int id = row * columns + col;
            float x = (col - (columns - 1) * 0.5f) * 0.95f;
            const float z = -6.0f - row * 1.10f;
            if (row % 2 == 1) x += 0.35f;
            drawAudiencePerson(x, z, id);
        }
    }
}

// ============================================================================
// MAIN 3D CONCERT SCENE ASSEMBLY
// ============================================================================
void drawConcertScene3D()
{
    // Elevated Stage Deck Height (1.35f) clears the front crowd barricade (0.90f)
    const float stageY = 1.35f;

    // 1. FEU Manila Grandstand & Athletic Field Environment (In Background at +Z)
    drawFEUManilaGrandstand();

    // 2. Rotate entire Concert Stage 180 degrees so its Front faces the Viewer (-Z)
    //    while keeping the FEU Manila Grandstand visible in the background
    glPushMatrix();
    glRotatef(180.0f, 0.0f, 1.0f, 0.0f);

    // Main Concert Stage Deck (Top at y = stageY = 1.35f)
    glPushMatrix();
    glTranslatef(0.0f, stageY * 0.5f, -1.0f);
    glScalef(20.0f, stageY, 7.6f);
    glColor3f(0.17f, 0.17f, 0.21f);
    glutSolidCube(1.0);
    glPopMatrix();

    // Stage Side Access Steps (5 steps climbing cleanly up to stageY)
    const int numSteps = 5;
    float stepH = stageY / numSteps;
    for (int side = -1; side <= 1; side += 2)
    {
        for (int step = 0; step < numSteps; step++)
        {
            float h = stepH * (step + 1);
            glPushMatrix();
            glTranslatef(side * (10.18f + (numSteps - 1 - step) * 0.34f), h * 0.5f, -0.5f);
            glScalef(0.35f, h, 1.4f);
            glColor3f(0.20f, 0.20f, 0.24f);
            glutSolidCube(1.0);
            glPopMatrix();
        }
    }

    // Elevated Center Drum Riser (Animated Translation via Up/Down Arrows)
    float totalRiserH = 0.35f + riserHeight;
    float riserTopY = stageY + totalRiserH;
    glPushMatrix();
    glTranslatef(0.0f, stageY + totalRiserH * 0.5f, -1.95f);
    glScalef(3.8f, totalRiserH, 2.7f);
    glColor3f(0.05f, 0.32f, 0.18f);
    glutSolidCube(1.0);
    glPopMatrix();

    // Stage Designs (Apron Chevrons, Wing Scrims, Light Totems, Crowd Barricade)
    drawStageDesigns(stageY);

    // Center 9x24 LED Wall + Left & Right 9x12 Rising Live-Feed LED Walls
    drawCenterLedWall9x24(stageY);
    drawRisingLiveFeedLedWalls();

    // Curved Dome Tent Truss & Tensioned Canopy
    drawDomeTentTruss();

    // Flown Line Array Speakers (8 Boxes per Side)
    drawLineArrayHang(-9.2f, 6.25f, 2.2f, 12.0f);
    drawLineArrayHang(9.2f, 6.25f, 2.2f, -12.0f);

    // Four Ground-Stacked Subwoofers on the Floor in Front of Stage
    drawFloorSubwoofer(-5.4f, 3.45f);
    drawFloorSubwoofer(-1.8f, 3.45f);
    drawFloorSubwoofer(1.8f, 3.45f);
    drawFloorSubwoofer(5.4f, 3.45f);

    // Full Band Instruments + Animated Musicians & Lead Singer
    drawDrumKit(0.0f, riserTopY, -1.80f);
    drawMusician(1, 0.0f, riserTopY, -2.42f, 0.0f, 0.04f, 0.38f, 0.18f);

    drawMicStandAndMonitor(0.0f, stageY, 1.62f, true);
    drawMusician(0, 0.0f, stageY, 1.24f, 0.0f, 0.12f, 0.12f, 0.15f);

    drawMusician(2, -3.45f, stageY, -0.05f, 24.0f, 0.04f, 0.42f, 0.20f);
    drawMicStandAndMonitor(-2.45f, stageY, 1.25f, true);
    glPushMatrix();
    glTranslatef(-4.35f, stageY + 0.45f, -1.35f);
    glRotatef(22.0f, 0.0f, 1.0f, 0.0f);
    glScalef(0.95f, 0.90f, 0.50f);
    glColor3f(0.14f, 0.14f, 0.16f);
    glutSolidCube(1.0);
    glPopMatrix();

    drawMusician(3, 3.65f, stageY, -0.15f, -24.0f, 0.15f, 0.15f, 0.18f);
    glPushMatrix();
    glTranslatef(4.45f, stageY + 0.475f, -1.35f);
    glRotatef(-22.0f, 0.0f, 1.0f, 0.0f);
    glScalef(1.0f, 0.95f, 0.52f);
    glColor3f(0.14f, 0.14f, 0.16f);
    glutSolidCube(1.0);
    glPopMatrix();

    drawKeyboardRig(2.25f, stageY, 1.02f, -22.0f);
    drawMusician(4, 2.08f, stageY, 0.56f, -22.0f, 0.85f, 0.66f, 0.12f);
    drawMicStandAndMonitor(1.45f, stageY, 1.38f, true);

    // Custom VBO Centerpiece: Spinning Textured Concert Crystal (Requirement 7)
    glPushMatrix();
    glTranslatef(0.0f, 6.75f + 0.16f * sinf(simTime * 2.5f), -0.9f);
    glRotatef(crystalSpinAngle, 0.0f, 1.0f, 0.0f);
    glRotatef(14.0f * sinf(simTime), 1.0f, 0.0f, 0.0f);

    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, backdropTextureID);
    glColor3f(0.95f, 0.95f, 1.0f);
    drawCrystalVBO();
    glDisable(GL_TEXTURE_2D);
    glPopMatrix();

    // ====================================================================
    // EXPANDED MULTI-ZONE MOVING-HEAD LIGHTING RIG (17 Moving Heads Total)
    // ====================================================================
    float panWave = (lightShowMode == 3) ? 18.0f : (38.0f * sinf(simTime * 1.6f));
    float crossSweep = (lightShowMode == 3) ? 16.0f : (28.0f * cosf(simTime * 2.1f));
    float floorFanSweep = (lightShowMode == 3) ? 12.0f : (26.0f * sinf(simTime * 2.4f));

    // Zone A: 5 Mid-Truss Moving-Head Spotlights (z = -0.9f)
    drawMovingHeadFixture(-7.2f, 7.50f, -0.9f, -panWave, 18.0f + 0.35f * spotSweepAngle,
                          0.0f, 0.95f, 1.0f, spotCyanOn, false, 2.0f, 7.0f);
    drawMovingHeadFixture(7.2f, 7.50f, -0.9f, panWave, 18.0f - 0.35f * spotSweepAngle,
                          0.0f, 0.95f, 1.0f, spotCyanOn, false, 2.0f, 7.0f);

    drawMovingHeadFixture(-3.6f, 8.90f, -0.9f, panWave * 0.8f, 16.0f - 0.45f * spotSweepAngle,
                          1.0f, 0.15f, 0.85f, spotMagentaOn, false, 2.1f, 7.8f);
    drawMovingHeadFixture(3.6f, 8.90f, -0.9f, -panWave * 0.8f, 16.0f + 0.45f * spotSweepAngle,
                          1.0f, 0.15f, 0.85f, spotMagentaOn, false, 2.1f, 7.8f);

    drawMovingHeadFixture(0.0f, 9.40f, -0.9f, panWave * 1.1f, 20.0f + sinf(simTime * 1.5f) * 18.0f,
                          1.0f, 0.78f, 0.10f, spotAmberOn, false, 2.2f, 8.2f);

    // Zone B: 4 Upstage Arch Truss Aerial Beam Moving Heads (z = -2.7f) [4]
    drawMovingHeadFixture(-5.8f, 8.10f, -2.7f, -crossSweep, 22.0f + 0.3f * spotSweepAngle,
                          0.08f, 0.95f, 0.48f, spotBeamsOn, false, 1.45f, 7.4f);
    drawMovingHeadFixture(-2.1f, 9.20f, -2.7f, crossSweep, 24.0f - 0.3f * spotSweepAngle,
                          0.98f, 0.78f, 0.12f, spotBeamsOn, false, 1.45f, 8.0f);
    drawMovingHeadFixture(2.1f, 9.20f, -2.7f, -crossSweep, 24.0f - 0.3f * spotSweepAngle,
                          0.98f, 0.78f, 0.12f, spotBeamsOn, false, 1.45f, 8.0f);
    drawMovingHeadFixture(5.8f, 8.10f, -2.7f, crossSweep, 22.0f + 0.3f * spotSweepAngle,
                          0.08f, 0.95f, 0.48f, spotBeamsOn, false, 1.45f, 7.4f);

    // Zone C: 8 Stage-Floor Mounted Moving Heads (y = stageY = 1.35f) [5]
    drawMovingHeadFixture(-8.4f, stageY, -3.05f, panWave * 0.8f, -22.0f + 0.45f * floorFanSweep,
                          0.08f, 0.95f, 0.48f, floorSpotsOn, true, 1.30f, 6.6f);
    drawMovingHeadFixture(-5.8f, stageY, -3.05f, -panWave * 0.8f, -18.0f - 0.45f * floorFanSweep,
                          0.98f, 0.78f, 0.12f, floorSpotsOn, true, 1.30f, 7.2f);
    drawMovingHeadFixture(5.8f, stageY, -3.05f, panWave * 0.8f, -18.0f - 0.45f * floorFanSweep,
                          0.98f, 0.78f, 0.12f, floorSpotsOn, true, 1.30f, 7.2f);
    drawMovingHeadFixture(8.4f, stageY, -3.05f, -panWave * 0.8f, -22.0f + 0.45f * floorFanSweep,
                          0.08f, 0.95f, 0.48f, floorSpotsOn, true, 1.30f, 6.6f);

    drawMovingHeadFixture(-8.6f, stageY, 2.15f, -crossSweep, -24.0f + 0.50f * floorFanSweep,
                          0.00f, 0.95f, 1.00f, floorSpotsOn, true, 1.25f, 6.4f);
    drawMovingHeadFixture(-4.6f, stageY, 2.35f, crossSweep, -16.0f - 0.50f * floorFanSweep,
                          1.00f, 0.15f, 0.85f, floorSpotsOn, true, 1.25f, 7.0f);
    drawMovingHeadFixture(4.6f, stageY, 2.35f, -crossSweep, -16.0f - 0.50f * floorFanSweep,
                          1.00f, 0.15f, 0.85f, floorSpotsOn, true, 1.25f, 7.0f);
    drawMovingHeadFixture(8.6f, stageY, 2.15f, crossSweep, -24.0f + 0.50f * floorFanSweep,
                          0.00f, 0.95f, 1.00f, floorSpotsOn, true, 1.25f, 6.4f);

    // Strobes, LED PAR Wash Cans, Stormy Blinders & LED Pixel Bars
    drawStrobeFixture(-3.4f, 8.70f, 0.9f, 28.0f);
    drawStrobeFixture(3.4f, 8.70f, 0.9f, 28.0f);
    drawStrobeFixture(-6.4f, 7.50f, -0.9f, 25.0f);
    drawStrobeFixture(6.4f, 7.50f, -0.9f, 25.0f);

    float parX[8] = {-7.2f, -5.2f, -3.2f, -1.1f, 1.1f, 3.2f, 5.2f, 7.2f};
    float parY[8] = {7.40f, 8.30f, 8.90f, 9.17f, 9.17f, 8.90f, 8.30f, 7.40f};
    for (int i = 0; i < 8; i++)
    {
        drawLedParFixture(parX[i], parY[i], 2.55f, 0.0f, 128.0f, i);
    }

    drawStormyLightFixture(-2.2f, 9.07f, 2.65f, -10.0f);
    drawStormyLightFixture(2.2f, 9.07f, 2.65f, 10.0f);
    drawStormyLightFixture(-7.8f, 7.10f, 0.90f, -22.0f);
    drawStormyLightFixture(7.8f, 7.10f, 0.90f, 22.0f);

    for (int b = 0; b < 6; b++)
    {
        float bx = -7.8f + b * 3.12f;
        drawLedPixelBar(bx, stageY - 0.07f, 2.84f, 0.0f, 2.8f);
    }
    drawLedPixelBar(-8.8f, 6.40f, 2.70f, 46.0f, 2.8f);
    drawLedPixelBar(8.8f, 6.40f, 2.70f, -46.0f, 2.8f);

    // Left & Right Stage Haze Machines + Center-Bound Volumetric Smoke
    drawHazeMachinesAndSmoke(stageY);

    glPopMatrix();
}

// ============================================================================
// REQUIREMENT 3: 2D ELEMENTS (Compact Orthographic HUD & Audio Equalizer)
// ============================================================================
void drawText2D(float x, float y, const std::string &text)
{
    glRasterPos2f(x, y);
    for (char c : text)
    {
        glutBitmapCharacter(GLUT_BITMAP_HELVETICA_12, c);
    }
}

void drawConcertHUD2D()
{
    glDisable(GL_LIGHTING);
    glDisable(GL_DEPTH_TEST);

    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    gluOrtho2D(0, winWidth, 0, winHeight);

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    if (showHUD)
    {
        const float left = 10.0f;
        const float right = 450.0f;
        const float top = (float)winHeight - 10.0f;
        const float bottom = (float)winHeight - 134.0f;

        glColor4f(0.03f, 0.11f, 0.07f, 0.68f);
        glBegin(GL_QUADS);
        glVertex2f(left, top);
        glVertex2f(right, top);
        glVertex2f(right, bottom);
        glVertex2f(left, bottom);
        glEnd();

        glColor3f(0.95f, 0.76f, 0.12f);
        glLineWidth(1.5f);
        glBegin(GL_LINE_LOOP);
        glVertex2f(left, top);
        glVertex2f(right, top);
        glVertex2f(right, bottom);
        glVertex2f(left, bottom);
        glEnd();

        std::string modeName = (lightShowMode == 1) ? "SWEEP" : (lightShowMode == 2) ? "PARTY"
                                                                                     : "FREEZE";

        glColor3f(0.95f, 0.78f, 0.15f);
        drawText2D(18.0f, top - 16.0f, "FEU MANILA GRANDSTAND - CONCERT CONSOLE");

        glColor3f(1.0f, 1.0f, 1.0f);
        drawText2D(18.0f, top - 33.0f, "[L] Mode: " + modeName + std::string(" | [0] Field: ") + (houseLightOn ? "ON" : "OFF") + " | [H] Hide HUD");
        drawText2D(18.0f, top - 50.0f,
                   std::string("[1] Cyan: ") + (spotCyanOn ? "ON " : "OFF ") +
                       std::string("[2] Mag: ") + (spotMagentaOn ? "ON " : "OFF ") +
                       std::string("[3] Amb: ") + (spotAmberOn ? "ON " : "OFF ") +
                       std::string("[4] Arch: ") + (spotBeamsOn ? "ON " : "OFF ") +
                       std::string("[5] Floor: ") + (floorSpotsOn ? "ON" : "OFF"));
        drawText2D(18.0f, top - 67.0f,
                   std::string("[F] Strobe: ") + (strobeOn ? "ON " : "OFF ") +
                       std::string("| [P] PARs: ") + (ledParsOn ? "ON " : "OFF ") +
                       std::string("| [E] Bars: ") + (ledBarsOn ? "ON" : "OFF"));
        drawText2D(18.0f, top - 84.0f,
                   std::string("[T] Stormy: ") + (stormyOn ? "ON " : "OFF ") +
                       std::string("| [B] Band: ") + (bandJamActive ? "ON " : "OFF ") +
                       std::string("| [M] Haze: ") + (hazeMachinesOn ? "ON" : "OFF"));
        drawText2D(18.0f, top - 101.0f, "[Up/Dn] Riser | [Drag] Orbit | [Wheel] Zoom");
        drawText2D(18.0f, top - 118.0f, "[WASD] Pan/Zoom | [R] Reset View");
    }
    else
    {
        glColor4f(0.03f, 0.11f, 0.07f, 0.65f);
        glBegin(GL_QUADS);
        glVertex2f(10.0f, winHeight - 10.0f);
        glVertex2f(122.0f, winHeight - 10.0f);
        glVertex2f(122.0f, winHeight - 32.0f);
        glVertex2f(10.0f, winHeight - 32.0f);
        glEnd();
        glColor3f(0.95f, 0.78f, 0.15f);
        drawText2D(18.0f, winHeight - 25.0f, "[H] Show Controls");
    }

    // Compact 2D Animated Audio Equalizer Bars (Bottom-Right)
    const int numBars = 14;
    float startX = (float)winWidth - 235.0f;
    float baseY = 14.0f;
    for (int i = 0; i < numBars; i++)
    {
        float wave = bandJamActive
                         ? (0.35f + 0.65f * fabsf(sinf(simTime * 5.2f + i * 0.65f)))
                         : 0.12f;
        float barHeight = 62.0f * wave;

        float ratio = (float)i / numBars;
        glColor4f(0.05f + 0.90f * ratio, 0.65f + 0.15f * ratio, 0.25f - 0.15f * ratio, 0.85f);
        glBegin(GL_QUADS);
        glVertex2f(startX + i * 16.0f, baseY);
        glVertex2f(startX + i * 16.0f + 11.0f, baseY);
        glVertex2f(startX + i * 16.0f + 11.0f, baseY + barHeight);
        glVertex2f(startX + i * 16.0f, baseY + barHeight);
        glEnd();
    }

    glDisable(GL_BLEND);

    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    glEnable(GL_DEPTH_TEST);
}

// ============================================================================
// DISPLAY, TIMER & RESHAPE CALLBACKS
// ============================================================================
void display()
{
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(45.0, (double)winWidth / (double)winHeight, 0.1, 160.0);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    float yawRad = camYaw * (float)M_PI / 180.0f;
    float pitchRad = camPitch * (float)M_PI / 180.0f;
    float eyeX = camPanX + camDistance * cosf(pitchRad) * sinf(yawRad);
    float eyeY = 2.8f + camDistance * sinf(pitchRad);
    float eyeZ = camDistance * cosf(pitchRad) * cosf(yawRad);

    gluLookAt(eyeX, eyeY, eyeZ,
              camPanX, 3.0f, 1.5f,
              0.0f, 1.0f, 0.0f);

    setupLighting();
    drawConcertScene3D();
    drawConcertCrowd();
    drawConcertHUD2D();

    glutSwapBuffers();
}

void updateTimer(int value)
{
    simTime += 0.016f;

    crystalSpinAngle += 1.2f;
    if (crystalSpinAngle >= 360.0f)
        crystalSpinAngle -= 360.0f;

    if (lightShowMode != 3)
    {
        spotSweepAngle = 32.0f * sinf(simTime * 1.8f);
    }

    if (bandJamActive)
    {
        bassPulse = 1.0f + 0.18f * fabsf(sinf(simTime * 7.5f));
    }
    else
    {
        bassPulse = 1.0f;
    }

    glutPostRedisplay();
    glutTimerFunc(16, updateTimer, 0);
}

void reshape(int w, int h)
{
    if (h == 0)
        h = 1;
    winWidth = w;
    winHeight = h;
    glViewport(0, 0, w, h);
}

// ============================================================================
// REQUIREMENT 2: GLUT KEYBOARD & MOUSE CALLBACKS
// ============================================================================
void keyboardCallback(unsigned char key, int x, int y)
{
    float rightX = cosf(camYaw * (float)M_PI / 180.0f);

    switch (key)
    {
    case '1':
        spotCyanOn = !spotCyanOn;
        break;
    case '2':
        spotMagentaOn = !spotMagentaOn;
        break;
    case '3':
        spotAmberOn = !spotAmberOn;
        break;
    case '4':
        spotBeamsOn = !spotBeamsOn;
        break;
    case '5':
        floorSpotsOn = !floorSpotsOn;
        break;
    case '0':
        houseLightOn = !houseLightOn;
        break;
    case 'f':
    case 'F':
        strobeOn = !strobeOn;
        break;
    case 'p':
    case 'P':
        ledParsOn = !ledParsOn;
        break;
    case 'e':
    case 'E':
        ledBarsOn = !ledBarsOn;
        break;
    case 't':
    case 'T':
        stormyOn = !stormyOn;
        break;
    case 'l':
    case 'L':
        lightShowMode = (lightShowMode % 3) + 1;
        break;
    case 'b':
    case 'B':
        bandJamActive = !bandJamActive;
        break;
    case 'm':
    case 'M':
        hazeMachinesOn = !hazeMachinesOn;
        break;
    case 'h':
    case 'H':
        showHUD = !showHUD;
        break;
    case 'a':
    case 'A':
        camPanX -= 0.5f * rightX;
        break;
    case 'd':
    case 'D':
        camPanX += 0.5f * rightX;
        break;
    case 'w':
    case 'W':
        camDistance = (camDistance > 6.0f) ? camDistance - 0.8f : 6.0f;
        break;
    case 's':
    case 'S':
        camDistance = (camDistance < 55.0f) ? camDistance + 0.8f : 55.0f;
        break;
    case 'r':
    case 'R':
        camYaw = 190.0f;
        camPitch = 21.0f;
        camDistance = 27.0f;
        camPanX = 0.0f;
        riserHeight = 0.0f;
        lightShowMode = 1;
        spotCyanOn = spotMagentaOn = spotAmberOn = spotBeamsOn = floorSpotsOn = houseLightOn = true;
        strobeOn = ledParsOn = ledBarsOn = stormyOn = bandJamActive = hazeMachinesOn = true;
        break;
    case 27:
        exit(0);
        break;
    }
    glutPostRedisplay();
}

void specialKeysCallback(int key, int x, int y)
{
    if (key == GLUT_KEY_UP && riserHeight < 1.2f)
    {
        riserHeight += 0.1f;
    }
    else if (key == GLUT_KEY_DOWN && riserHeight > 0.0f)
    {
        riserHeight -= 0.1f;
    }
    glutPostRedisplay();
}

void mouseCallback(int button, int state, int x, int y)
{
    lastMouseX = x;
    lastMouseY = y;

    if (button == GLUT_LEFT_BUTTON)
    {
        isDraggingLeft = (state == GLUT_DOWN);
    }
    else if (button == GLUT_RIGHT_BUTTON)
    {
        isDraggingRight = (state == GLUT_DOWN);
    }
    else if (button == 3 && state == GLUT_DOWN)
    {
        camDistance = (camDistance > 6.0f) ? camDistance - 1.0f : 6.0f;
    }
    else if (button == 4 && state == GLUT_DOWN)
    {
        camDistance = (camDistance < 55.0f) ? camDistance + 1.0f : 55.0f;
    }
    glutPostRedisplay();
}

void mouseMotionCallback(int x, int y)
{
    int dx = x - lastMouseX;
    int dy = y - lastMouseY;

    if (isDraggingLeft)
    {
        camYaw += dx * 0.4f;
        camPitch += dy * 0.3f;
        if (camPitch > 75.0f)
            camPitch = 75.0f;
        if (camPitch < 3.0f)
            camPitch = 3.0f;
    }
    else if (isDraggingRight)
    {
        camDistance += dy * 0.10f;
        if (camDistance < 6.0f)
            camDistance = 6.0f;
        if (camDistance > 55.0f)
            camDistance = 55.0f;
    }

    lastMouseX = x;
    lastMouseY = y;
    glutPostRedisplay();
}

// ============================================================================
// MAIN ENTRY POINT
// ============================================================================
int main(int argc, char **argv)
{
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
    glutInitWindowSize(winWidth, winHeight);
    glutCreateWindow("CS0045 Final Project - FEU Manila Grandstand Concert & Light Show");

    GLenum err = glewInit();
    if (err != GLEW_OK)
    {
        fprintf(stderr, "GLEW Initialization Error: %s\n", glewGetErrorString(err));
        return 1;
    }

    glClearColor(0.02f, 0.03f, 0.06f, 1.0f);
    glEnable(GL_DEPTH_TEST);

    initCrystalVBO();
    backdropTextureID = loadConcertTexture("backdrop.bmp");
    liveFeedTextureID = loadLiveFeedTexture("livefeed.bmp");

    glutDisplayFunc(display);
    glutReshapeFunc(reshape);
    glutKeyboardFunc(keyboardCallback);
    glutSpecialFunc(specialKeysCallback);
    glutMouseFunc(mouseCallback);
    glutMotionFunc(mouseMotionCallback);
    glutTimerFunc(16, updateTimer, 0);

    glutMainLoop();
    return 0;
}