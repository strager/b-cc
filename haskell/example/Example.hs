import Control.Monad
import Foreign.C.String (CString, peekCString, withCString)
import Foreign.Marshal.Utils (with)
import Foreign.Ptr (Ptr, nullPtr)
import Foreign.Storable (peek)
import System.Exit (exitFailure, exitSuccess)
import System.FilePath (dropExtension)
import System.IO (hPutStrLn, stderr, stdout)
import System.Process (rawSystem)

import B.Foreign.BuildContext (BuildContext)
import B.Foreign.Exception (Exception)
import B.Foreign.FileRule (FileRuleCallback)
import B.Foreign.Rule (AnyRule)

import qualified B.Foreign.BuildContext as BuildContext
import qualified B.Foreign.DatabaseInMemory as DatabaseInMemory
import qualified B.Foreign.FileQuestion as FileQuestion
import qualified B.Foreign.FileRule as FileRule
import qualified B.Foreign.UUID as UUID

cObjectFiles :: [String]
cObjectFiles
  = [ "lib/Answer.c.o"
    , "lib/BuildContext.c.o"
    , "lib/Database.c.o"
    , "lib/FileQuestion.c.o"
    , "lib/FileRule.c.o"
    , "lib/Portable.c.o"
    , "lib/Question.c.o"
    , "lib/QuestionVTableList.c.o"
    , "lib/Rule.c.o"
    , "lib/RuleQueryList.c.o"
    , "lib/Serialize.c.o"
    , "lib/UUID.c.o"
    , "example/Example.c.o"
    ]

ccObjectFiles :: [String]
ccObjectFiles
  = [ "lib/DatabaseInMemory.cc.o"
    , "lib/Exception.cc.o"
    ]

objectFiles :: [String]
objectFiles = cObjectFiles ++ ccObjectFiles

outputFile :: String
outputFile = "b-cc-example-in-hs"

runCommand
  :: Ptr (Ptr Exception)
  -> String
  -> [String]
  -> IO ()
runCommand _exPtr command args = do
  hPutStrLn stdout $ "$ " ++ unwords (command : args)
  _exitCode <- rawSystem command args
  -- TODO Handle _exitCode.
  return ()

needFile
  :: Ptr (Ptr Exception)
  -> Ptr BuildContext
  -> String
  -> IO ()
needFile exPtr ctx filePath
  = withCString filePath $ \ filePathPtr -> do
    question <- FileQuestion.allocate filePathPtr
    questionVTable <- FileQuestion.vtable
    BuildContext.needOne ctx question questionVTable exPtr

runCCompile
  :: Ptr BuildContext
  -> CString
  -> Ptr (Ptr Exception)
  -> IO ()
runCCompile ctx oFilePathPtr exPtr = do
  oFilePath <- peekCString oFilePathPtr
  let cFilePath = dropExtension oFilePath
  needFile exPtr ctx cFilePath
  -- TODO Handle exception.
  runCommand exPtr "clang"
    [ "-Ilib"
    , "-o", oFilePath
    , "-c"
    , cFilePath
    ]

runCCCompile
  :: Ptr BuildContext
  -> CString
  -> Ptr (Ptr Exception)
  -> IO ()
runCCCompile ctx oFilePathPtr exPtr = do
  oFilePath <- peekCString oFilePathPtr
  let ccFilePath = dropExtension oFilePath
  needFile exPtr ctx ccFilePath
  -- TODO Handle exception.
  runCommand exPtr "clang++"
    [ "-Ilib"
    , "-std=c++11"
    , "-stdlib=libc++"
    , "-o", oFilePath
    , "-c"
    , ccFilePath
    ]

runCCLink
  :: Ptr BuildContext
  -> CString
  -> Ptr (Ptr Exception)
  -> IO ()
runCCLink ctx oFilePathPtr exPtr = do
  oFilePath <- peekCString oFilePathPtr
  mapM_ (needFile exPtr ctx) objectFiles
  -- TODO Handle exception.
  runCommand exPtr "clang++"
    $ [ "-stdlib=libc++"
      , "-o", oFilePath
      ] ++ objectFiles

addFileRule
  :: Ptr AnyRule
  -> String
  -> FileRuleCallback
  -> IO ()
addFileRule rule cFilePath callback
  = withCString cFilePath $ \ filePath -> do
    callbackPtr <- FileRule.mkFileRuleCallback callback
    FileRule.add rule filePath callbackPtr

main :: IO ()
main = do
  -- TODO Deserialize database from file.
  database <- DatabaseInMemory.allocate
  databaseVTable <- DatabaseInMemory.vtable

  rule <- FileRule.allocate
  ruleVTable <- FileRule.vtable

  forM_ cObjectFiles $ \ path
    -> addFileRule rule path runCCompile
  forM_ ccObjectFiles $ \ path
    -> addFileRule rule path runCCCompile
  addFileRule rule outputFile runCCLink

  question <- withCString outputFile FileQuestion.allocate
  questionVTable <- FileQuestion.vtable

  ctx <- BuildContext.allocate
    database databaseVTable
    rule ruleVTable

  with nullPtr $ \ exPtr -> do
  BuildContext.needOne ctx question questionVTable exPtr
  ex <- peek exPtr
  unless (ex == nullPtr) $ do
    hPutStrLn stderr "Exception occured while building"
    exitFailure

  -- TODO Deallocate.
  -- TODO Serialize database to file.

  UUID.deallocateLeaked
  exitSuccess
