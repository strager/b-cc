{-# LANGUAGE ForeignFunctionInterface #-}

module B.Foreign.Serialize where

import Foreign.C
import Foreign.Ptr

type Serializer closure
  = Ptr CChar -> CSize -> Ptr closure -> IO ()

type Deserializer closure
  = Ptr CChar -> CSize -> Ptr closure -> IO CSize

type SerializeFunc closure a
  = Ptr a -> Serializer closure -> Ptr closure -> IO ()

type DeserializeFunc0 closure a
  = Deserializer closure -> Ptr closure -> IO ()

type DeserializeFunc userClosure closure a
  = Ptr userClosure -> DeserializeFunc0 closure a
