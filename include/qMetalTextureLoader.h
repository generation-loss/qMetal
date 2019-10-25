#ifndef __GL_TEXTURE_LOADER_H__
#define __GL_TEXTURE_LOADER_H__

#include "qMetalTypes.h"
#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

namespace qMetal
{    
    class TextureLoader
    {
    public:
        
        static qMetal::Texture*    LoadTexture(NSString *name, const qMetal::Texture::Params *params = NULL);
        static qMetal::Texture*    LoadTexture(const char *name, const qMetal::Texture::Params *params = NULL);
        static qMetal::Texture*    TextureFromText(NSString *text, UIFont *font, const qRGBA &colour, const qMetal::Texture::Params *params = NULL);
        static qMetal::Texture*    TextureFromUIImage(UIImage *image, const CGRect &crop, const CGSize &scale, const qMetal::Texture::Params *params = NULL);
        static void             UnloadTexture(qMetal::Texture *texture);
    
    private:
        
        static qMetal::Texture*    UIImageToTexture(UIImage *image, const qMetal::Texture::Params *params);
        
    };
}

#endif //__GL_TEXTURE_LOADER_H__
