/** Collect files from a browser drop (single files, multi-select, or folders). */

interface DropFileEntry {
  isFile: boolean;
  isDirectory: boolean;
  name: string;
  file: (success: (file: File) => void, error?: (err: DOMException) => void) => void;
}

interface DropDirectoryEntry extends DropFileEntry {
  createReader: () => {
    readEntries: (
      success: (entries: DropFileEntry[]) => void,
      error?: (err: DOMException) => void,
    ) => void;
  };
}

async function readDirectoryEntries(dir: DropDirectoryEntry): Promise<DropFileEntry[]> {
  const reader = dir.createReader();
  const out: DropFileEntry[] = [];
  for (;;) {
    const batch = await new Promise<DropFileEntry[]>((resolve, reject) => {
      reader.readEntries(resolve, reject);
    });
    if (batch.length === 0) {
      break;
    }
    out.push(...batch);
  }
  return out;
}

async function walkEntry(entry: DropFileEntry, files: File[]): Promise<void> {
  if (entry.isFile) {
    const file = await new Promise<File>((resolve, reject) => {
      entry.file(resolve, reject);
    });
    files.push(file);
    return;
  }
  if (!entry.isDirectory) {
    return;
  }
  const children = await readDirectoryEntries(entry as DropDirectoryEntry);
  for (const child of children) {
    await walkEntry(child, files);
  }
}

export async function collectDropFiles(dataTransfer: DataTransfer): Promise<File[]> {
  const direct = Array.from(dataTransfer.files);
  if (direct.length > 0) {
    return direct;
  }

  const items = dataTransfer.items;
  if (!items || items.length === 0) {
    return [];
  }

  const files: File[] = [];
  for (const item of Array.from(items)) {
    if (item.kind !== 'file') {
      continue;
    }
    const entry = (item as DataTransferItem & { webkitGetAsEntry?: () => DropFileEntry | null })
      .webkitGetAsEntry?.() as DropFileEntry | null;
    if (entry) {
      await walkEntry(entry, files);
      continue;
    }
    const file = item.getAsFile();
    if (file) {
      files.push(file);
    }
  }
  return files;
}
