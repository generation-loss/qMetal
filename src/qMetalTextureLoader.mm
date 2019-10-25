
#import "qMetal.h"
#import "qCore.h"
#import <QuartzCore/QuartzCore.h>
#import "UIImage+qCategories.h"

namespace qMetal
{    
    qMetal::Texture* TextureLoader::LoadTexture(NSString *name, const qMetal::Texture::Params *params)
    {
        const char *cName = [name cStringUsingEncoding:NSUTF8StringEncoding];
        return LoadTexture( cName, params );
    }
    
    qMetal::Texture* TextureLoader::LoadTexture(const char *name, const qMetal::Texture::Params *params)
    {
        qSPAM("LoadTexture: loading %s", name);
                
        NSString *nsName = [NSString stringWithCString:name encoding:NSASCIIStringEncoding];
        NSRange extensionRange = [nsName rangeOfString:@"." options:NSBackwardsSearch];
        NSString *nsExtension = [nsName substringFromIndex:(extensionRange.location+1)];
        
        qMetal::Texture *texture;            
        qMetal::Texture::Params defaultParams;
        if ( !params )
        {
            params = &defaultParams;
        }
        
        if ( [nsExtension caseInsensitiveCompare:@"png"] == NSOrderedSame )
        {
            qASSERTM( params->pixelFormat == qMetal::Texture::Params::ePixelFormat_RGBA8, "Only RGBA8 texture loading supported" );        
            UIImage *image = [UIImage imageNamed:nsName];
            qASSERTM( image, "Failed to load image %s", name );
            return UIImageToTexture( image, params );            
        }
        else if ( [nsExtension caseInsensitiveCompare:@"pvrtc"] == NSOrderedSame )
        {
            qBREAK("qLoadTexture: PVRTC not implemented");
            //texture = LoadPVRTC(name, params);;
        }
        else 
        {
            qBREAK("Image %s isn't a .png nor .pvrtc", name);        
        }
        
        return texture;        
    }
    
    qMetal::Texture* TextureLoader::TextureFromUIImage(UIImage *uiImage, const CGRect &crop, const CGSize &scale, const qMetal::Texture::Params *params )
    {
        UIImage *croppedScaledImage = [uiImage crop:crop andScale:scale];        
        qMetal::Texture::Params defaultParams;
        if ( !params )
        {
            params = &defaultParams;
        }
        return UIImageToTexture( croppedScaledImage, params );
    }
    
    qMetal::Texture* TextureLoader::UIImageToTexture( UIImage *uiImage, const qMetal::Texture::Params *params )
    {
        CGImageRef image = uiImage.CGImage;
        
        size_t width = CGImageGetWidth(image);
        size_t height = CGImageGetHeight(image);
        
        if ( !qIsPowerOfTwo(width) || !qIsPowerOfTwo(height) )
        {
            qASSERTM( params->wrapX == qMetal::Texture::Params::eWrap_ClampToEdge, "Texture wrap modes must be eWrap_ClampToEdge for non power-of-two textures" );
            qASSERTM( params->wrapY == qMetal::Texture::Params::eWrap_ClampToEdge, "Texture wrap modes must be eWrap_ClampToEdge for non power-of-two textures" );
            qASSERTM( params->minFilter == qMetal::Texture::Params::eMinFilter_Linear || params->minFilter == qMetal::Texture::Params::eMinFilter_Point, "Texture min filter must be eMinFilter_Point || eMinFilter_Linear for non power-of-two textures" );
        }
        
        qASSERTM( width >= 64, "Texture width is not at least 64" ); // lame!
        qASSERTM( height >= 64, "Texture height is not at least 64" ); // lame!
        
        GLubyte* data = (GLubyte*)calloc(width * height * 4, sizeof( GLubyte ) );
        
        CGContextRef context = CGBitmapContextCreate(data, width, height, 8, width * 4, CGColorSpaceCreateDeviceRGB(), kCGImageAlphaPremultipliedLast | kCGBitmapByteOrder32Big );
        qASSERTM( context != NULL, "Couldn't create context" );
        CGContextSetInterpolationQuality( context, kCGInterpolationHigh );
        CGContextSetShouldAntialias( context, YES );
        CGContextDrawImage( context, CGRectMake(0.0, 0.0, (CGFloat)width, (CGFloat)height), image );
        CGContextRelease( context );        
            
        /*
         for ( int i = 0; i < 50 * 4; i+=4 )
         {
         qSPAM( "%u, %u, %u, %u", (uint)data[i], (uint)data[i+1], (uint)data[i+2], (uint)data[i+3] );
         }
         */
        
        qMetal::Texture *texture = new qMetal::Texture();
        glGenTextures(1, &(texture->texture));
        qMetal::Device::Dispatch( *texture, qMetal::TextureUnit_0 );
        
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
        
        qMetal::util::CheckGLError();
        
        if ( params->generateMipMaps ) 
        {
            glGenerateMipmap(GL_TEXTURE_2D);
            qMetal::util::CheckGLError();
        }
        
        free(data);    
        
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, params->generateMipMaps ? params->GetMinMipFilter() : params->minFilter );
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, params->magFilter);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, params->wrapX);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, params->wrapY);
        
        qMetal::util::CheckGLError();
        
        texture->width = width;
        texture->height = height;
        texture->params = *params;
        
        return texture;
    }
    
    qMetal::Texture* TextureLoader::TextureFromText(NSString *text, UIFont *font, const qRGBA &colour, const qMetal::Texture::Params *params)
    {
        qASSERTM( text != nil, "No text given" );
        qASSERTM( font != nil, "No UIFont given" );
        
        qMetal::Texture::Params defaultParams;
        if ( !params )
        {
            params = &defaultParams;
        }
        
        qMetal::util::CheckGLError();
        
        CGSize textSize = [text sizeWithFont:font];
        
        size_t width = qPowerOfTwo( textSize.width );
        size_t height = qPowerOfTwo( textSize.height );
        
        width = ( width < 64 ) ? 64 : width;
        height = ( height < 64 ) ? 64 : height;
                
        CGRect labelRect = CGRectMake( 0, 0, width, height );
        UILabel *label = [[UILabel alloc] initWithFrame:labelRect];
        [label setFont:font];
        [label setText:text];
        [label setTextColor:[UIColor colorWithRed:colour.r green:colour.g blue:colour.b alpha:colour.a]];
        [label setTextAlignment:UITextAlignmentCenter];
                
        GLubyte* data = (GLubyte*)calloc(width * height * 4, sizeof( GLubyte ) );
        
        CGContextRef context = CGBitmapContextCreate(data, width, height, 8, width * 4, CGColorSpaceCreateDeviceRGB(), kCGImageAlphaPremultipliedLast | kCGBitmapByteOrder32Big);
        qASSERTM( context != NULL, "Couldn't create context for %s", [text cStringUsingEncoding:NSUTF8StringEncoding] );
        CGContextSetShouldSmoothFonts( context, YES );
        [label setClearsContextBeforeDrawing:NO];
        [label setBackgroundColor:[UIColor colorWithRed:1.0f green:1.0f blue:1.0f alpha:0.0f]];
        [label.layer renderInContext:context];
        CGContextRelease(context);    
        
        [label release];
            
        qMetal::Texture *texture = new qMetal::Texture();
        glGenTextures(1, &(texture->texture));
        qMetal::Device::Dispatch( *texture, qMetal::TextureUnit_0 );
        
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
        if ( params->generateMipMaps ) 
        {
            glGenerateMipmap(GL_TEXTURE_2D);
        }
        
        free(data);    
        
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, params->generateMipMaps ? params->GetMinMipFilter() : params->minFilter );
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, params->magFilter);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, params->wrapX);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, params->wrapY);
        
        qMetal::util::CheckGLError();
        
        texture->width = width;
        texture->height = height;
        texture->params = *params;
        
        return texture;        
    }
    
    void TextureLoader::UnloadTexture(qMetal::Texture *texture)
    {
        glDeleteTextures( 1, &(texture->texture) );
    }

    /*
    #define PVR_TEXTURE_FLAG_TYPE_MASK  0xff

    static const char *gPVRTexIdentifier = "PVR!";

    enum
    {
        kPVRTextureFlagTypePVRTC_2 = 24,
        kPVRTextureFlagTypePVRTC_4
    };

    typedef struct _PVRTexHeader
    {
        uint headerLength;
        uint height;
        uint width;
        uint numMipmaps;
        uint flags;
        uint dataLength;
        uint bpp;
        uint bitmaskRed;
        uint bitmaskGreen;
        uint bitmaskBlue;
        uint bitmaskAlpha;
        uint pvrTag;
        uint numSurfs;
    } PVRTexHeader;

    + (qMetal::Texture)LoadPVRTC:(const char*)cName withParams:(qMetal::Texture::Params)params
    {
        qMetal::util::CheckGLError();
        
        uint pvrtcSize;
        ubyte *data = (ubyte*)qLoadData(cName, pvrtcSize);  
        
        PVRTexHeader *header = (PVRTexHeader *)data;
        
        uint pvrTag = CFSwapInt32LittleToHost(header->pvrTag);
        
        if (gPVRTexIdentifier[0] != ((pvrTag >>  0) & 0xff) ||
            gPVRTexIdentifier[1] != ((pvrTag >>  8) & 0xff) ||
            gPVRTexIdentifier[2] != ((pvrTag >> 16) & 0xff) ||
            gPVRTexIdentifier[3] != ((pvrTag >> 24) & 0xff))
        {
            qBREAK("qLoadTexture: invalide pvrTag");
        }
        
        uint flags = CFSwapInt32LittleToHost(header->flags);
        uint formatFlags = flags & PVR_TEXTURE_FLAG_TYPE_MASK;
        
        qASSERTM( formatFlags == kPVRTextureFlagTypePVRTC_4 || formatFlags == kPVRTextureFlagTypePVRTC_2, "LoadPVRTC: unknown PVRTC format flags" );
        
        GLenum internalFormat;
        
        if (formatFlags == kPVRTextureFlagTypePVRTC_4)
            internalFormat = GL_COMPRESSED_RGBA_PVRTC_4BPPV1_IMG;
        else if (formatFlags == kPVRTextureFlagTypePVRTC_2)
            internalFormat = GL_COMPRESSED_RGBA_PVRTC_2BPPV1_IMG;
        
        uint width = CFSwapInt32LittleToHost(header->width);
        uint height = CFSwapInt32LittleToHost(header->height);
        qSPAM( "LoadPVRTC: width = %u, height = %u; mip maps = %u", width, height, CFSwapInt32LittleToHost(header->numMipmaps) );
        
        //if (CFSwapInt32LittleToHost(header->bitmaskAlpha))
        //_hasAlpha = TRUE;
        //else
        //_hasAlpha = FALSE;
        //
        
        uint dataLength = CFSwapInt32LittleToHost(header->dataLength);
        uint dataOffset = 0;
        uint mipmap = 0;
        
        ubyte *bytes = data + sizeof(PVRTexHeader);
        
        qMetal::Texture texture;
        glGenTextures(1, &(texture.texture));
        qMetal::Device::Dispatch( texture, qMetal::TextureUnit_0 );
        
        // Calculate the data size for each texture level and respect the minimum number of blocks
        while (dataOffset < dataLength)
        {
            uint blockSize = 0, widthBlocks = 0, heightBlocks = 0, bpp = 4;
            
            if (formatFlags == kPVRTextureFlagTypePVRTC_4)
            {
                blockSize = 4 * 4; // Pixel by pixel block size for 4bpp
                widthBlocks = width / 4;
                heightBlocks = height / 4;
                bpp = 4;
            }
            else
            {
                blockSize = 8 * 4; // Pixel by pixel block size for 2bpp
                widthBlocks = width / 8;
                heightBlocks = height / 4;
                bpp = 2;
            }
            
            // Clamp to minimum number of blocks
            if (widthBlocks < 2)
                widthBlocks = 2;
            if (heightBlocks < 2)
                heightBlocks = 2;
            
            ubyte *imageData = bytes+dataOffset;
            uint dataSize = widthBlocks * heightBlocks * ((blockSize  * bpp) / 8);
            
            glCompressedTexImage2D(GL_TEXTURE_2D, mipmap, internalFormat, width, height, 0, dataSize, imageData ); 
            
            dataOffset += dataSize;
            ++mipmap;                
            
            width = MAX(width >> 1, 1);
            height = MAX(height >> 1, 1);
        }
        
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, mipmap > 1 ? params.GetMinMipFilter() : params.minFilter );
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, params.magFilter);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, params.wrapX);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, params.wrapY);
        
        qMetal::util::CheckGLError();
        
        return texture;
    }
    */
}
