// WBQtPreviewImage.h -- the shared conversion for ObjectPreview bridge buffers (the
// 128x128 BGR bottom-up model renders handed over by WBQtObject_/WBQtFence_/WBQtGrove_/
// WBQtPickUnit_RenderPreview).
//
// The MFC ObjectPreview::DrawMyTexture does NOT blit the whole render: it stretches only
// the CENTER QUARTER (source rect (w/4, h/4, w/2, h/2)) into the control -- a deliberate
// 2x zoom into the model (see the commented-out full-blit line above it). Every Qt
// preview must apply the same crop or the model looks half-sized vs MFC.
#ifndef WB_QT_PREVIEW_IMAGE_H
#define WB_QT_PREVIEW_IMAGE_H

#include <QImage>
#include <QPixmap>

namespace WBQtPreviewImage
{
	// bgr = w*h*3 bytes, bottom-up rows (the MFC path relied on the positive-height DIB
	// blit to flip). Returns the top-down RGB image cropped to the MFC center quarter.
	// (The crop is symmetric, so bottom-up vs top-down does not shift it.)
	inline QImage fromBridgeBgr(const unsigned char *bgr, int w, int h)
	{
		QImage img(w, h, QImage::Format_RGB888);
		for (int y = 0; y < h; ++y)
		{
			const unsigned char *src = bgr + (h - 1 - y) * w * 3;
			unsigned char *dst = img.scanLine(y);
			for (int x = 0; x < w; ++x)
			{
				dst[x * 3 + 0] = src[x * 3 + 2];	// R <- B
				dst[x * 3 + 1] = src[x * 3 + 1];	// G
				dst[x * 3 + 2] = src[x * 3 + 0];	// B <- R
			}
		}
		return img.copy(w / 4, h / 4, w / 2, h / 2);
	}

	// Fit the cropped image into the preview label. The crop is 64x64 and the labels are
	// bigger, so this is an UPSCALE: use nearest-neighbour like the MFC StretchDIBits
	// (its default COLORONCOLOR mode) -- bilinear smoothing turns the model into a blur.
	inline QPixmap toLabelPixmap(const QImage &img, const QSize &labelSize)
	{
		return QPixmap::fromImage(img).scaled(labelSize,
			Qt::KeepAspectRatio, Qt::FastTransformation);
	}
}

#endif // WB_QT_PREVIEW_IMAGE_H
