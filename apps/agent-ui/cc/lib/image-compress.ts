const MAX_DIMENSION = 1536;
const JPEG_QUALITY = 0.8;

export type CompressedImage = {
  base64: string;
  mediaType: 'image/jpeg' | 'image/png' | 'image/gif' | 'image/webp';
};

const SUPPORTED_TYPES = new Set([
  'image/jpeg', 'image/png', 'image/gif', 'image/webp',
]);

function blobToBase64(blob: Blob): Promise<string> {
  return new Promise((resolve, reject) => {
    const reader = new FileReader();
    reader.onloadend = () => {
      const dataUrl = reader.result as string;
      const comma = dataUrl.indexOf(',');
      resolve(dataUrl.slice(comma + 1));
    };
    reader.onerror = () => reject(reader.error ?? new Error('FileReader failed'));
    reader.readAsDataURL(blob);
  });
}

export type CompressOptions = {
  compress?: boolean;
};

export async function compressImage(file: File | Blob, opts?: CompressOptions): Promise<CompressedImage> {
  const fileType = file.type;
  if (fileType === 'image/heic' || fileType === 'image/heif') {
    throw new Error('HEIC/HEIF images are not supported. Convert to JPEG or PNG first.');
  }
  if (fileType && fileType.startsWith('image/') && !SUPPORTED_TYPES.has(fileType)) {
    throw new Error(`Unsupported image format: ${fileType}`);
  }

  const compress = opts?.compress !== false;

  const bitmap = await createImageBitmap(file);
  const { width, height } = bitmap;

  const needsResize = compress && (width > MAX_DIMENSION || height > MAX_DIMENSION);

  let targetWidth = width;
  let targetHeight = height;
  if (needsResize) {
    const scale = MAX_DIMENSION / Math.max(width, height);
    targetWidth = Math.round(width * scale);
    targetHeight = Math.round(height * scale);
  }

  if (!needsResize) {
    const originalType = fileType;
    if (
      originalType === 'image/jpeg' ||
      originalType === 'image/png' ||
      originalType === 'image/gif' ||
      originalType === 'image/webp'
    ) {
      const base64 = await blobToBase64(file);
      bitmap.close();
      return { base64, mediaType: originalType };
    }
  }

  const canvas = new OffscreenCanvas(targetWidth, targetHeight);
  const ctx = canvas.getContext('2d')!;
  ctx.drawImage(bitmap, 0, 0, targetWidth, targetHeight);
  bitmap.close();

  const blob = await canvas.convertToBlob({ type: 'image/jpeg', quality: JPEG_QUALITY });
  const base64 = await blobToBase64(blob);

  return { base64, mediaType: 'image/jpeg' };
}
