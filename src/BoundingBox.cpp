/*
LibBoubek/BoundingBox.cpp
Copyright (c) 2003-2008, Tamy Boubekeur

All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

* Redistributions of source code must retain the above copyright notice,
  this list of conditions and the following disclaimer.

* Redistributions in binary form must reproduce the above copyright notice,
  this list of conditions and the following disclaimer in the documentation
  and/or other materials provided with the distribution.

* The name of the source code copyright owner cannot be used to endorse or
  promote products derived from this software without specific prior
  written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include "BoundingBox.h"

using namespace std;
/*void drawLine(Vec3Df u, Vec3Df v);

void drawLine(Vec3Df u, Vec3Df v){

    glVertex3f(u[0], u[1], u[2]);
    glVertex3f(v[0], v[1], v[2]);
};

void BoundingBox::drawBoundingBox(){
    glBegin(GL_LINES);

    glColor3f(1.0,1.0,1.0);
    Vec3Df e1 = Vec3Df(1.0,0.0,0.0);
    Vec3Df e2 = Vec3Df(0.0,1.0,0.0);
    Vec3Df e3 = Vec3Df(0.0,0.0,1.0);

    float u = this->getLength();
    float v = this->getWidth();
    float w = this->getHeight();

    drawLine(getMin(),getMin()+u*e1); //1
    drawLine(getMin(),getMin()+v*e2);
    drawLine(getMin(),getMin()+w*e3);

    drawLine(getMin() +u*e1,getMin() +u*e1 +v*e2);
    drawLine(getMin() +u*e1,getMin() +u*e1 +w*e3); //5

    drawLine(getMin() +v*e2,getMin() +u*e2 +v*e3);
    drawLine(getMin() +v*e2,getMin() +u*e2 +u*e1);

    drawLine(getMin() +w*e3,getMin() +w*e3 +u*e1);
    drawLine(getMin() +w*e3,getMin() +w*e3 +v*e2);

    drawLine(getMax() ,getMax()-u*e1); //10
    drawLine(getMax() ,getMax()-v*e2);
    drawLine(getMax() ,getMax()-w*e3);


};
*/
static inline  void drawLine(const vector<Vec3Df> & V, int i, int j);
inline  void drawLine (const vector<Vec3Df> & V, int i, int j) {
    glVertex3f(V[i][0], V[i][1], V[i][2]);
    glVertex3f(V[j][0], V[j][1], V[j][2]);
}

void BoundingBox::renderGL () const {
    glBegin (GL_LINES);

    vector<Vec3Df> V(8);
    vector<Triangle> T(12);


    Vec3Df whl = maxBb - minBb;

    // (x, y, z)
    V[0] = minBb + Vec3Df(0, 0, 1) * whl[2];
    V[1] = maxBb - Vec3Df(0, 1, 0) * whl[1];
    V[2] = maxBb - Vec3Df(1, 0, 0) * whl[0];
    V[3] = maxBb;
    V[4] = minBb;
    V[5] = minBb + Vec3Df(1, 0, 0) * whl[0];
    V[6] = minBb + Vec3Df(0, 1, 0) * whl[1];
    V[7] = maxBb - Vec3Df(0, 0, 1) * whl[2];

    vector<pair<int, int> > lines;
    drawLine(V, 1, 0);
    drawLine(V, 2, 0);
    drawLine(V, 3, 1);
    drawLine(V, 3, 2);
    drawLine(V, 4, 0);
    drawLine(V, 5, 1);
    drawLine(V, 5, 4);
    drawLine(V, 6, 2);
    drawLine(V, 6, 4);
    drawLine(V, 7, 3);
    drawLine(V, 7, 5);
    drawLine(V, 7, 6);

    glEnd ();

}
