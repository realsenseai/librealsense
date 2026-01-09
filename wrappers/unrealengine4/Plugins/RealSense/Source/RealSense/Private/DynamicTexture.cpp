#include "DynamicTexture.h"
#include "PCH.h"

FDynamicTexture::FDynamicTexture(FString Name, 
	int Width, int Height, EPixelFormat Format, TextureCompressionSettings Compression) 
{
	SCOPED_PROFILER;

	REALSENSE_TRACE(TEXT("+FDynamicTexture %p %s"), this, *Name);

	this->Name = Name;
	this->Width = Width;
	this->Height = Height;
	this->Bpp = GPixelFormats[Format].BlockBytes;
	this->Format = Format;
	this->Compression = Compression;

	CommandCounter.Increment();
#if ENGINE_MAJOR_VERSION >= 4 && ENGINE_MINOR_VERSION >= 27
	this->RenderCmd_CreateTexture();
#else
	ENQUEUE_RENDER_COMMAND(CreateTextureCmd)(
		[this](FRHICommandListImmediate& RHICmdList)
		{
			this->RenderCmd_CreateTexture();
		});
#endif
}

FDynamicTexture::~FDynamicTexture()
{
	SCOPED_PROFILER;

	REALSENSE_TRACE(TEXT("~FDynamicTexture %p %s CommandCounter=%d TudPool=%d"), this, *Name, (int)CommandCounter.GetValue(), (int)TudPool.size());

	{
		const float Timeout = 5;
		const float Granularity = 200.0f / 1000.0f;
		int NumTries = (int)(Timeout / Granularity);

		NAMED_PROFILER("WaitCommandCounter");
		while (CommandCounter.GetValue() > 0 && NumTries > 0)
		{
			//REALSENSE_ERR(TEXT("Wait FDynamicTexture CommandCounter=%d"), (int)CommandCounter.GetValue());
			FPlatformProcess::Sleep(Granularity);
			NumTries--;
		}
	}

	if (TextureObject)
	{
		TextureObject->RemoveFromRoot();
		TextureObject = nullptr;
	}

	for (FTextureUpdateData* Tud : TudPool)
	{
		if (Tud->Data) FMemory::Free(Tud->Data);
		FMemory::Free(Tud);
	}
	TudPool.clear();
}

FTextureUpdateData* FDynamicTexture::AllocBuffer()
{
	SCOPED_PROFILER;

	if (!TudPool.empty())
	{
		FScopeLock Lock(&TudMx);
		if (!TudPool.empty())
		{
			auto Tud = TudPool.back();
			TudPool.pop_back();
			return Tud;
		}
	}

	auto Tud = (FTextureUpdateData*)FMemory::Malloc(sizeof(FTextureUpdateData), PLATFORM_CACHE_LINE_SIZE);
	Tud->Context = this;
	Tud->DataSize = Width * Height * Bpp;
	Tud->Data = FMemory::Malloc(Tud->DataSize, PLATFORM_CACHE_LINE_SIZE);
	Tud->Stride = Width * Bpp;
	Tud->Width = Width;
	Tud->Height = Height;

	return Tud;
}

void FDynamicTexture::EnqueUpdateCommand(FTextureUpdateData* Tud)
{
	SCOPED_PROFILER;

	// UE4 is so great!
	const bool HackIsValidThread = (!GIsThreadedRendering || ENamedThreads::GetRenderThread() != ENamedThreads::GameThread);

	if (!HackIsValidThread)
	{
		REALSENSE_ERR(TEXT("EnqueUpdateCommand: invalid thread"));
	}
	else if (TextureObject && TextureObject->Resource)
	{
		CommandCounter.Increment();
		ENQUEUE_RENDER_COMMAND(UpdateTextureCmd)(
			[Tud](FRHICommandListImmediate& RHICmdList)
			{
				Tud->Context->RenderCmd_UpdateTexture(Tud);
			});
	}
}

void FDynamicTexture::Update(const rs2::video_frame& Frame)
{
	SCOPED_PROFILER;

	const auto fw = Frame.get_width();
	const auto fh = Frame.get_height();
	const auto fbpp = Frame.get_bytes_per_pixel();

	if (Width != fw || Height != fh || Bpp != fbpp)
	{
		REALSENSE_ERR(TEXT("Invalid video_frame: %s Width=%d Height=%d Bpp=%d"), 
			*Name, fw, fh, fbpp);
		return;
	}

	auto* Tud = AllocBuffer();
	CopyData(Tud, Frame);
	EnqueUpdateCommand(Tud);
}

void FDynamicTexture::CopyData(FTextureUpdateData* Tud, const rs2::video_frame& Frame)
{
	SCOPED_PROFILER;

	const auto fw = Frame.get_width();
	const auto fh = Frame.get_height();
	const auto fbpp = Frame.get_bytes_per_pixel();
	const auto fs = Frame.get_stride_in_bytes();

	uint8_t* Dst = (uint8_t*)Tud->Data;
	const uint8_t* Src = (const uint8_t*)Frame.get_data();

	for (int y = 0; y < fh; ++y)
	{
		FMemory::Memcpy(Dst, Src, fw * fbpp);
		Dst += (fw * fbpp);
		Src += fs;
	}
}

void FDynamicTexture::RenderCmd_CreateTexture()
{
	SCOPED_PROFILER;

	REALSENSE_TRACE(TEXT("FDynamicTexture::RenderCmd_CreateTexture %p %s"), this, *Name);

	auto Tex = UTexture2D::CreateTransient(Width, Height, Format);
	if (!Tex)
	{
		REALSENSE_ERR(TEXT("UTexture2D::CreateTransient failed"));
	}
	else
	{
		#if WITH_EDITORONLY_DATA
			Tex->MipGenSettings = TextureMipGenSettings::TMGS_NoMipmaps;
		#endif

		Tex->CompressionSettings = Compression;
		Tex->Filter = TextureFilter::TF_Nearest;
		Tex->SRGB = 0;

		Tex->AddToRoot();
		Tex->UpdateResource();

		TextureObject = Tex;
	}

	CommandCounter.Decrement();
}

void FDynamicTexture::RenderCmd_UpdateTexture(FTextureUpdateData* Tud)
{
	SCOPED_PROFILER;

	auto Tex = Tud->Context->TextureObject;
	if (Tex && Tex->Resource)
	{
		RHIUpdateTexture2D(
			((FTexture2DResource*)Tex->Resource)->GetTexture2DRHI(), 
			0, 
			FUpdateTextureRegion2D(0, 0, 0, 0, Tud->Width, Tud->Height), 
			Tud->Stride, 
			(const uint8*)Tud->Data
		);
	}

	{
		FScopeLock Lock(&TudMx);
		TudPool.push_back(Tud);
	}

	CommandCounter.Decrement();
}
