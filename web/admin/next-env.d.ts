/// <reference types="next" />
/// <reference types="next/image-types/global" />

// CSS module and plain CSS import declarations
declare module '*.css' {
  const styles: { readonly [key: string]: string };
  export default styles;
}

// NOTE: This file should not be edited
// see https://nextjs.org/docs/app/building-your-application/configuring/typescript for more information.
