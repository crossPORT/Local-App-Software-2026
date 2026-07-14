/// <reference types="vite/client" />
/// <reference path="../../../sdks/typescript/src/webusb.d.ts" />

interface ImportMetaEnv {
  readonly VITE_ROCKETBOX_RELEASE_TAG?: string;
  readonly VITE_BASE_PATH?: string;
}

interface ImportMeta {
  readonly env: ImportMetaEnv;
}
