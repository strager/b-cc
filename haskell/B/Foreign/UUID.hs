{-# LANGUAGE EmptyDataDecls #-}
{-# LANGUAGE ForeignFunctionInterface #-}

module B.Foreign.UUID where

import Foreign.C

type UUID = CString

foreign import ccall "b_uuid_from_stable_string"
  fromStableString
    :: CString
    -> IO UUID

foreign import ccall "b_uuid_from_temp_string"
  fromTempString
    :: CString
    -> IO UUID

foreign import ccall "b_uuid_deallocate_leaked"
  deallocateLeaked
    :: IO ()

foreign import ccall "b_uuid_equal"
  equal
    :: UUID
    -> UUID
    -> IO Bool

foreign import ccall "b_uuid_compare"
  compare
    :: UUID
    -> UUID
    -> IO CInt
