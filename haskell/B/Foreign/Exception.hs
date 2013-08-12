{-# LANGUAGE EmptyDataDecls #-}
{-# LANGUAGE ForeignFunctionInterface #-}

module B.Foreign.Exception where

import Foreign.C
import Foreign.Ptr

data Exception

foreign import ccall "b_exception_string"
  string
    :: CString
    -> IO (Ptr Exception)

foreign import ccall "b_exception_deallocate"
  deallocate
    :: Ptr Exception
    -> IO ()

foreign import ccall "b_exception_aggregate"
  aggregate
    :: Ptr (Ptr Exception)
    -> CSize
    -> IO (Ptr Exception)
