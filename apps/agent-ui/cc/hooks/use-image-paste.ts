import { useState, useEffect, useRef, useCallback } from 'react';
import { toast } from 'sonner';
import { compressImage, type CompressedImage } from '../lib/image-compress.js';
import { useImageSizeLimit, imageSizeLimitBytes, IMAGE_SIZE_LIMIT_LABELS } from './use-image-size-limit.js';
import { useImageCompress } from './use-image-compress.js';
import type { UserContent, UserContentBlock } from '../shared/ai.types.js';

export function useImagePaste() {
  const [pendingImages, setPendingImages] = useState<CompressedImage[]>([]);
  const [isDragging, setIsDragging] = useState(false);
  const [imageSizeLimit] = useImageSizeLimit();
  const [imageCompress] = useImageCompress();
  const dragCounterRef = useRef(0);

  const addFiles = useCallback(async (files: Iterable<File>) => {
    for (const file of files) {
      if (!file.type.startsWith('image/')) continue;
      try {
        const compressed = await compressImage(file, { compress: imageCompress });
        const maxBytes = imageSizeLimitBytes(imageSizeLimit);
        if (maxBytes !== null) {
          const sizeBytes = Math.ceil(compressed.base64.length * 3 / 4);
          if (sizeBytes > maxBytes) {
            const sizeMB = (sizeBytes / (1024 * 1024)).toFixed(1);
            toast.error(`Image too large (${sizeMB} MB). Limit: ${IMAGE_SIZE_LIMIT_LABELS[imageSizeLimit]}. Change in Settings.`);
            continue;
          }
        }
        setPendingImages(prev => [...prev, compressed]);
      } catch (err) {
        toast.error(err instanceof Error ? err.message : 'Failed to process image');
      }
    }
  }, [imageSizeLimit, imageCompress]);

  const handlePaste = useCallback(async (e: React.ClipboardEvent<HTMLTextAreaElement>) => {
    const items = e.clipboardData.items;
    const imageFiles: File[] = [];
    for (let i = 0; i < items.length; i++) {
      if (items[i].type.startsWith('image/')) {
        const file = items[i].getAsFile();
        if (file) imageFiles.push(file);
      }
    }
    if (imageFiles.length === 0) return;
    e.preventDefault();
    await addFiles(imageFiles);
  }, [addFiles]);

  const handleDrop = useCallback(async (e: React.DragEvent) => {
    e.preventDefault();
    e.stopPropagation();
    dragCounterRef.current = 0;
    setIsDragging(false);
    const files = Array.from(e.dataTransfer.files).filter(f => f.type.startsWith('image/'));
    if (files.length > 0) await addFiles(files);
  }, [addFiles]);

  useEffect(() => {
    const hasFiles = (e: DragEvent) => e.dataTransfer?.types.includes('Files') ?? false;

    const onDragEnter = (e: DragEvent) => {
      if (!hasFiles(e)) return;
      e.preventDefault();
      dragCounterRef.current++;
      if (dragCounterRef.current === 1) setIsDragging(true);
    };
    const onDragLeave = (e: DragEvent) => {
      if (!hasFiles(e)) return;
      dragCounterRef.current--;
      if (dragCounterRef.current === 0) setIsDragging(false);
    };
    const onDragOver = (e: DragEvent) => {
      if (!hasFiles(e)) return;
      e.preventDefault();
    };
    const onDrop = (_e: DragEvent) => {
      dragCounterRef.current = 0;
      setIsDragging(false);
    };

    document.addEventListener('dragenter', onDragEnter);
    document.addEventListener('dragleave', onDragLeave);
    document.addEventListener('dragover', onDragOver);
    document.addEventListener('drop', onDrop);
    return () => {
      document.removeEventListener('dragenter', onDragEnter);
      document.removeEventListener('dragleave', onDragLeave);
      document.removeEventListener('dragover', onDragOver);
      document.removeEventListener('drop', onDrop);
    };
  }, []);

  const removePendingImage = (index: number) => {
    setPendingImages(prev => prev.filter((_, i) => i !== index));
  };

  const clearPendingImages = () => setPendingImages([]);

  const buildContent = (text: string): UserContent => {
    const hasImages = pendingImages.length > 0;
    const hasText = text.length > 0;
    if (!hasImages) return text;
    const blocks: UserContentBlock[] = [];
    if (hasText) blocks.push({ type: 'text', text });
    for (const img of pendingImages) {
      blocks.push({ type: 'image', source: { type: 'base64', media_type: img.mediaType, data: img.base64 } });
    }
    return blocks;
  };

  return { pendingImages, isDragging, handlePaste, handleDrop, removePendingImage, clearPendingImages, buildContent };
}

/** The value returned by {@link useImagePaste}. A page-level owner creates ONE
 *  instance (for its drop overlay) and passes it down to the `ChatComposer` it
 *  renders, so the drop target and the composer that shows/sends thumbnails
 *  share the SAME `pendingImages` state instead of two isolated copies. */
export type ImagePasteApi = ReturnType<typeof useImagePaste>;
