/*
 Copyright (C) 2016 by Wojciech Jaśkowski, Michał Kempka, Grzegorz Runc, Jakub Toczek, Marek Wydmuch

 Permission is hereby granted, free of charge, to any person obtaining a copy
 of this software and associated documentation files (the "Software"), to deal
 in the Software without restriction, including without limitation the rights
 to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:

 The above copyright notice and this permission notice shall be included in
 all copies or substantial portions of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 THE SOFTWARE.
*/

#include "viz_labels.h"
#include "viz_defines.h"
#include "v_video.h"
#include "p_lnspec.h"

#ifdef VIZ_LABELS_TEST
#include <SDL_events.h>
#endif

VIZLabelsBuffer* vizLabels = NULL;

VIZLabelsBuffer::VIZLabelsBuffer(unsigned int width, unsigned int height):
        bufferSize(height * width), bufferWidth(width), bufferHeight(height) {

    buffer = new BYTE[bufferSize];
    memset(buffer, 0, bufferSize);

    this->labeled = VIZ_MAX_SEG_LABELS;
    this->currentLabel = 0;
    this->currentSprite = nullptr;

    this->currentSegmentLabel = 0;
    this->exitSignId = -1;
    FTextureID exitSign = TexMan.CheckForTexture("EXITSIGN", FTexture::TEX_Any);
    if (exitSign.Exists())
        this->exitSignId = exitSign.GetIndex();

    // SDL debug stuff
    #ifdef VIZ_LABELS_TEST
        for(int j = 0; j < 256; j++)
        {
            #ifndef VIZ_LABELS_COLORS
                colors[j].r = colors[j].g = colors[j].b = j;
            #else
                colors[j].r= j%3==0 ? 255 : 0;
                colors[j].g= j%3==1 ? 255 : 0;
                colors[j].b= j%3==2 ? 255 : 0;
                if(j%2==0)
                    colors[j].r=colors[j].g=colors[j].b = j;
                if(j%7==0){
                    colors[j].r = 100;
                    colors[j].g = 200;
                    colors[j].b = 255;

                }
            #endif
        }
        this->window = SDL_CreateWindow("ViZDoom Labels Buffer", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, width, height, SDL_WINDOW_SHOWN);
        this->surface =  SDL_GetWindowSurface(window);
    #endif
}

VIZLabelsBuffer::~VIZLabelsBuffer() {
    delete[] buffer;

    #ifdef VIZ_LABELS_TEST
        SDL_DestroyWindow(this->window);
        window = NULL;
        SDL_FreeSurface(this->surface);
        surface = NULL;
    #endif
}

// Get labels buffer pointer
BYTE* VIZLabelsBuffer::getBuffer() { return buffer; }

// Get pointer for requested pixel (x, y coords)
BYTE* VIZLabelsBuffer::getBufferPoint(unsigned int x, unsigned int y) {
    if( x < bufferWidth && y < bufferHeight )
        return buffer + x + y * bufferWidth;
    else return NULL;
}

// Set point(x,y) value with next label
void VIZLabelsBuffer::setPoint(unsigned int x, unsigned int y) {
    if (this->currentLabel != 0)
        this->setPoint(x, y, this->currentLabel);
    else
        this->setSegPoint(x, y);
    this->updateBoundingBox(x, y);
}

void VIZLabelsBuffer::setSegPoint(unsigned int x, unsigned int y)
{
    this->setPoint(x, y, this->currentSegmentLabel);
    this->updateBoundingBox(x, y);
}

// Set point(x,y) value with requested label
inline void VIZLabelsBuffer::setPoint(unsigned int x, unsigned int y, BYTE label) {
    BYTE *map = getBufferPoint(x, y);
    if(map != NULL) *map = label;
}

// Get buffer size
unsigned int VIZLabelsBuffer::getBufferSize(){
    return this->bufferSize;
}

// Get buffer width
unsigned int VIZLabelsBuffer::getBufferWidth(){
    return bufferWidth;
}

// Get buffer height
unsigned int VIZLabelsBuffer::getBufferHeight(){
    return bufferHeight;
}

// Clear buffer - set every point to 0
void VIZLabelsBuffer::clearBuffer() {
    this->clearBuffer(0);
}

// Set every point to color value
void VIZLabelsBuffer::clearBuffer(BYTE color) {
    memset(buffer, color, bufferSize);

    this->sprites.clear();
    this->labeled = VIZ_MAX_SEG_LABELS;
    this->pSprite = NULL;
    this->unsetSprite();
    this->segments.clear();
    this->unsetSegment();
}

void VIZLabelsBuffer::lock() {this->locked=true; }

void VIZLabelsBuffer::unlock() {this->locked=false; }

bool VIZLabelsBuffer::isLocked() { return this->locked; }

void VIZLabelsBuffer::sizeUpdate() {
    if(this->bufferWidth != (unsigned int)screen->GetWidth() || this->bufferHeight!=(unsigned int)screen->GetHeight()) {
        delete[] this->buffer;
        this->bufferHeight=(unsigned)screen->GetHeight();
        this->bufferWidth= (unsigned)screen->GetWidth();
        this->bufferSize=this->bufferHeight*this->bufferWidth;
        this->buffer = new BYTE[this->bufferSize];

        #ifdef VIZ_LABELS_TEST
            SDL_SetWindowSize(this->window, this->bufferWidth,this->bufferHeight);
            this->surface=SDL_GetWindowSurface(this->window);
        #endif
    }
}

void VIZLabelsBuffer::updateBoundingBox(unsigned int x, unsigned int y){
    if(!this->currentSprite)
        return;

    if(x < this->currentSprite->minX) this->currentSprite->minX = x;
    if(x > this->currentSprite->maxX) this->currentSprite->maxX = x;
    if(y < this->currentSprite->minY) this->currentSprite->minY = y;
    if(y > this->currentSprite->maxY) this->currentSprite->maxY = y;
    ++this->currentSprite->pointCount;
}

void VIZLabelsBuffer::addSprite(AActor *actor, vissprite_t* vis){
    VIZSprite sprite;
    sprite.actor = actor;
    sprite.actorId = this->getActorId(actor);
    sprite.vissprite = vis;

    this->sprites.push_back(sprite);
}

void VIZLabelsBuffer::addPSprite(AActor *actor, vissprite_t* vis) {
    if (this->pSprite != NULL) {
        this->pSprite->vissprite = vis;
    } else {
        VIZSprite sprite;
        sprite.actor = actor;
        sprite.actorId = 0;
        sprite.psprite = true;
        sprite.vissprite = vis;

        this->sprites.push_back(sprite);
        this->pSprite = &this->sprites[this->sprites.size() - 1];
    }
}

BYTE VIZLabelsBuffer::getLabel(VIZSprite* sprite){
    if(sprite->label > 0 && sprite->labeled)
        return sprite->label;

    if(sprite->psprite) sprite->label = VIZ_MAX_LABELS - 1;
    else{
        ++labeled;
        sprite->label = labeled;
    }
    sprite->labeled = true;
    return sprite->label;
}


BYTE VIZLabelsBuffer::getLabel(vissprite_t* vis){
    for(auto i = this->sprites.begin(); i != this->sprites.end(); ++i){
        if(i->vissprite == vis){
            return this->getLabel(&(*i));
        }
    }
    return 0;
}

unsigned int VIZLabelsBuffer::getActorId(AActor *actor){
    auto actorId = this->actors.find(actor);
    if(actorId != this->actors.end()){
        return actorId->second;
    }
    else{
        unsigned int newId = this->actors.size() + 1;
        this->actors.insert({actor, newId});
        return newId;
    }
}

void VIZLabelsBuffer::clearActors(){
    this->actors.clear();
}

std::vector<VIZSprite> VIZLabelsBuffer::getSprites(){
    return this->sprites;
}

void VIZLabelsBuffer::setLabel(BYTE label){
    this->currentLabel = label;
}

void VIZLabelsBuffer::setSprite(vissprite_t* vis){
    for(auto i = this->sprites.begin(); i != this->sprites.end(); ++i){
        if(i->vissprite == vis){
            this->currentLabel = this->getLabel(&(*i));
            this->currentSprite = &(*i);
            return;
        }
    }
    this->currentLabel = 0;
    this->currentSprite = NULL;
}

void VIZLabelsBuffer::unsetSprite(){
    this->currentLabel = 0;
    this->currentSprite = NULL;
}

void VIZLabelsBuffer::setSegment(seg_t *seg)
{
    if ( seg == NULL )
        return;

    BYTE label;

    if (seg->linedef != NULL) {
        int special = seg->linedef->special;
        int	*args = seg->linedef->args;
        if (special == Teleport_NewMap || special == Teleport_EndGame ||
            special == Exit_Normal || special == Exit_Secret)
        {
            label = 1;
            auto iter = this->segments.find(label);
            if ( iter == this->segments.end())
                this->segments[label] = "Exit";
            this->currentSegmentLabel = label;

            return;
        }

        if ( special == Door_LockedRaise || special == ACS_LockedExecute || special == ACS_LockedExecuteDoor ||
             (special == Door_Animated ) || (special == Generic_Door ) ||
             (special == FS_Execute ) || special == Door_Open || special == Door_Close ||
                special == Door_CloseWaitOpen || special == Door_Raise)
        {
            label = 2;
            auto iter = this->segments.find(label);
            if ( iter == this->segments.end())
                this->segments[label] = "Door";
            this->currentSegmentLabel = label;

            return;
        }

        if ( LineSpecialsInfo[special] != NULL && LineSpecialsInfo[special]->max_args >= 0 &&
             special != Door_Open && special != Door_Close && special != Door_CloseWaitOpen &&
             special != Door_Raise && special != Door_Animated && special != Generic_Door )
        {
            label = 3;
            auto iter = this->segments.find(label);
            if ( iter == this->segments.end())
                this->segments[label] = "Switch";
            this->currentSegmentLabel = label;

            return;
        }
    }

    if (this->exitSignId >= 0 && seg->sidedef != NULL) {
        side_t *side = seg->sidedef;
        for (size_t i = 0; i < countof(side->textures); i++) {
            if (side->textures[i].texture.GetIndex() == this->exitSignId) {

                label = 4;
                auto iter = this->segments.find(label);
                if ( iter == this->segments.end())
                    this->segments[label] = "ExitSign";
                this->currentSegmentLabel = label;

                return;
            }
        }
    }
}

void VIZLabelsBuffer::unsetSegment()
{
    this->currentSegmentLabel = 0;
}

// Store x value for later usage
void VIZLabelsBuffer::storeX(int x) {this->tX=x; }

// Store y value for later usage
void VIZLabelsBuffer::storeY(int y) {this->tY=y; }

// Get stored x value
int VIZLabelsBuffer::getX(void) {return this->tX; }

// Get stored y value
int VIZLabelsBuffer::getY(void) {return this->tY; }


#ifdef VIZ_LABELS_TEST
// Update labels debug window
void VIZLabelsBuffer::testUpdate() {
    SDL_Surface* surf = SDL_CreateRGBSurfaceFrom(this->buffer, this->bufferWidth, this->bufferHeight, 8, this->bufferWidth, 0, 0, 0, 0);
    SDL_SetPaletteColors(surf->format->palette, colors, 0, 256);
    SDL_BlitSurface(surf, NULL, this->surface, NULL);
    SDL_UpdateWindowSurface(this->window);
}
#endif