/** Read a File into a Uint8Array (browser File API). */
export async function readFilePayload(file: File): Promise<Uint8Array> {
  const buffer = await file.arrayBuffer();
  return new Uint8Array(buffer);
}
