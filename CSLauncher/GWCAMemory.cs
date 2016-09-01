using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Runtime.InteropServices;
using System.Diagnostics;
using System.IO;

namespace GWCA
{
    namespace Memory
    {
        class GWCAMemory
        {
            #region Basic Members
            // GwProcess we will use
            private Process process;

            // Scan variables.
            private IntPtr scan_start;
            private int scan_size;
            private byte[] memory_dump;
            #endregion

            #region Constructor
            // Constructor
            public GWCAMemory(Process proc)
            {
                scan_start = IntPtr.Zero;
                scan_size = 0;
                memory_dump = null;
                process = proc;
            }
            #endregion

            #region PInvokes
            // PInvokes
            [DllImport("kernel32.dll", SetLastError = true)]
            static extern bool WriteProcessMemory(
                IntPtr hProcess,
                IntPtr lpBaseAddress,
                IntPtr lpBuffer,
                int nSize,
                out IntPtr lpNumberOfBytesWritten);

            [DllImport("kernel32.dll", SetLastError = true)]
            static extern bool ReadProcessMemory(
                IntPtr hProcess,
                IntPtr lpBaseAddress,
                IntPtr lpBuffer,
                int nSize,
                out IntPtr lpNumberOfBytesRead);

            [DllImport("kernel32.dll")]
            static extern IntPtr CreateRemoteThread(
               IntPtr hProcess,
               IntPtr lpThreadAttributes,
               uint dwStackSize,
               IntPtr lpStartAddress,
               IntPtr lpParameter,
               uint dwCreationFlags,
               out IntPtr lpThreadId);

            [DllImport("kernel32.dll", SetLastError = true, ExactSpelling = true)]
            static extern IntPtr VirtualAllocEx(
                IntPtr hProcess,
                IntPtr lpAddress,
                IntPtr dwSize,
                uint dwAllocationType,
                uint dwProtect);

            [DllImport("kernel32.dll", SetLastError = true, ExactSpelling = true)]
            static extern IntPtr VirtualFreeEx(
                IntPtr hProcess,
                IntPtr lpAddress,
                uint dwSize,
                uint dwFreeType);

            [DllImport("kernel32", CharSet = CharSet.Ansi, ExactSpelling = true, SetLastError = true)]
            static extern IntPtr GetProcAddress(IntPtr hModule, string procName);

            [DllImport("kernel32.dll", SetLastError = true)]
            static extern UInt32 WaitForSingleObject(IntPtr hHandle, UInt32 dwMilliseconds);

            [DllImport("kernel32.dll", SetLastError = true)]
            static extern UInt32 GetExitCodeThread(IntPtr hHandle, out IntPtr dwMilliseconds);

            [DllImport("kernel32.dll", CharSet = CharSet.Auto)]
            public static extern IntPtr GetModuleHandle(string lpModuleName);
            #endregion

            #region Basic Memory Functions
            /// <summary>
            /// Read T value from memory address.
            /// </summary>
            /// <typeparam name="T">Type of value to read.</typeparam>
            /// <param name="address">Address to read from.</param>
            /// <returns>Found value.</returns>
            public T Read<T>(IntPtr address)
            {
                IntPtr bytesread;
                int size = Marshal.SizeOf(typeof(T));
                IntPtr buffer = Marshal.AllocHGlobal(size);

                ReadProcessMemory(process.Handle,
                                  address,
                                  buffer,
                                  size,
                                  out bytesread
                                  );

                T ret = (T)Marshal.PtrToStructure(buffer, typeof(T));
                Marshal.FreeHGlobal(buffer);

                return ret;
            }

            /// <summary>
            /// Read array of bytes from memory. Used in scanner.
            /// </summary>
            /// <param name="address">Address of base to read from.</param>
            /// <param name="size">Amount of bytes to read from base.</param>
            /// <returns>bytes read.</returns>
            public byte[] ReadBytes(IntPtr address, int size)
            {
                IntPtr bytesread;
                IntPtr buffer = Marshal.AllocHGlobal(size);

                ReadProcessMemory(process.Handle,
                                  address,
                                  buffer,
                                  size,
                                  out bytesread
                                  );

                byte[] ret = new byte[size];
                Marshal.Copy(buffer, ret, 0, size);
                Marshal.FreeHGlobal(buffer);

                return ret;
            }

            /// <summary>
            /// Read a unicode string from memory.
            /// </summary>
            /// <param name="address">Address of string base.</param>
            /// <param name="maxsize">Max possible known size of string.</param>
            /// <returns>String found.</returns>
            public string ReadWString(IntPtr address, int maxsize)
            {
                byte[] rawbytes = ReadBytes(address, maxsize);
                string ret = Encoding.Unicode.GetString(rawbytes);
                if (ret.Contains("\0"))
                    ret = ret.Substring(0, ret.IndexOf('\0'));
                return ret;
            }

            /// <summary>
            /// Read through and traverse a heap allocated object for specific values.
            /// Base read first before applying offset each iteration of traversal.
            /// </summary>
            /// <typeparam name="T">Type of value to retrieve at end.</typeparam>
            /// <param name="Base">Base address to start multilevel pointer traversal.</param>
            /// <param name="offsets">Array of offsets to use to traverse to desired memory location.</param>
            /// <returns></returns>
            public T ReadPtrChain<T>(IntPtr Base, params int[] offsets)
            {
                foreach (int offset in offsets)
                    Base = Read<IntPtr>(Base) + offset;
                return Read<T>(Base);
            }

            public void Write<T>(IntPtr address, T data)
            {
                IntPtr bytesread;
                int size = Marshal.SizeOf(typeof(T));
                IntPtr buffer = new IntPtr();
                Marshal.StructureToPtr(data, buffer, true);

                WriteProcessMemory(
                            process.Handle,
                            address,
                            buffer,
                            size,
                            out bytesread);
            }

            public void WriteBytes(IntPtr address, byte[] data)
            {
                IntPtr bytesread;
                int size = data.Length;
                IntPtr buffer = Marshal.AllocHGlobal(size);
                Marshal.Copy(data, 0, buffer, size);

                WriteProcessMemory(
                            process.Handle,
                            address,
                            buffer,
                            size,
                            out bytesread);

                Marshal.FreeHGlobal(buffer);
            }

            public void WriteWString(IntPtr address, string data)
            {
                WriteBytes(address, Encoding.Unicode.GetBytes(data));
            }
            #endregion

            #region Memory Scanner
            /// <summary>
            /// Initialize scanner range, dump memory block for scan.
            /// </summary>
            /// <param name="startaddr"></param>
            /// <param name="size"></param>
            /// <returns></returns>
            public bool InitScanner(IntPtr startaddr, int size)
            {

                if (process == null) return false;
                if (scan_start != IntPtr.Zero) return false;

                scan_start = startaddr;
                scan_size = size;

                memory_dump = ReadBytes(startaddr, size);

                return true;
            }

            /// <summary>
            /// Scan memory block for byte signature matches.
            /// </summary>
            /// <param name="signature">Group of bytes to match</param>
            /// <param name="offset">Offset from matched sig to pointer.</param>
            /// <returns>Address found if sucessful, IntPtr.Zero if not.</returns>
            public IntPtr ScanForPtr(byte[] signature, int offset = 0, bool readptr = false) {
                bool match;
                byte first = signature[0];
                int sig_length = signature.Length;

                // For start to end of scan range...
                for (int scan = 0; scan < scan_size; ++scan) {
                    // Skip iteration if first byte does not match
                    if (memory_dump[scan] != first) {
                        continue;
                    }

                    match = true;

                    // For sig size... check for matching signature.
                    for (int sig = 0; sig < sig_length; ++sig) {
                        if (memory_dump[scan + sig] != signature[sig]) {
                            match = false;
                            break;
                        }
                    }

                    // Add scanned address to base, plus desired offset, and read the address stored.
                    if (match) {
						if (readptr) {
                            return new IntPtr(BitConverter.ToUInt32(memory_dump, scan + offset));
						} else {
                            return new IntPtr(scan_start.ToInt32() + scan + offset);
						}
					}
                }
                return IntPtr.Zero;
            }

            /// <summary>
            /// Deallocate memory block and scan range of scanner.
            /// </summary>
            public void TerminateScanner()
            {
                scan_start = IntPtr.Zero;
                scan_size = 0;
                memory_dump = null;
            }
            #endregion

            #region Module Injection

            public enum LOADMODULERESULT : uint
            {
                SUCCESSFUL,
                MODULE_NONEXISTANT,
                KERNEL32_NOT_FOUND,
                LOADLIBRARY_NOT_FOUND,
                MEMORY_NOT_ALLOCATED,
                PATH_NOT_WRITTEN,
                REMOTE_THREAD_NOT_SPAWNED,
                REMOTE_THREAD_DID_NOT_FINISH,
                MEMORY_NOT_DEALLOCATED
            }

            /// <summary>
            /// Inject module into process using LoadLibrary CRT method.
            /// </summary>
            /// <param name="modulepath">Relative path to module to load.</param>
            /// <returns>bool on if injection was sucessful</returns>
            public LOADMODULERESULT LoadModule(string modulepath,out IntPtr module)
            {
                string modulefullpath = Path.GetFullPath(modulepath);
                module = IntPtr.Zero;

                if (!File.Exists(modulefullpath))
                {
                    return LOADMODULERESULT.MODULE_NONEXISTANT;
                }

                IntPtr hKernel32 = GetModuleHandle("kernel32.dll");
                if (hKernel32 == IntPtr.Zero)
                {
                    return LOADMODULERESULT.KERNEL32_NOT_FOUND;
                }

                IntPtr hLoadLib = GetProcAddress(hKernel32, "LoadLibraryW");
                if (hLoadLib == IntPtr.Zero)
                {
                    return LOADMODULERESULT.LOADLIBRARY_NOT_FOUND;
                }

                IntPtr hStringBuffer = VirtualAllocEx(process.Handle, IntPtr.Zero, new IntPtr(2 * (modulefullpath.Length + 1)), 0x3000 /* MEM_COMMIT | MEM_RESERVE */, 0x4 /* PAGE_READWRITE */);
                if (hStringBuffer == IntPtr.Zero)
                {
                    return LOADMODULERESULT.MEMORY_NOT_ALLOCATED;
                }

                WriteWString(hStringBuffer, modulefullpath);
                if (ReadWString(hStringBuffer, 260) != modulefullpath)
                {
                    return LOADMODULERESULT.PATH_NOT_WRITTEN;
                }

                IntPtr hThread = CreateRemoteThread(process.Handle, IntPtr.Zero, 0, hLoadLib, hStringBuffer, 0, out hThread);
                if (hThread == IntPtr.Zero)
                {
                    return LOADMODULERESULT.REMOTE_THREAD_NOT_SPAWNED;
                }

                uint ThreadResult = WaitForSingleObject(hThread, 5000);
                if (ThreadResult == 0x102 /* WAIT_TIMEOUT */ || ThreadResult == 0xFFFFFFFF /* WAIT_FAILED */)
                {
                    return LOADMODULERESULT.REMOTE_THREAD_DID_NOT_FINISH;
                }

                if(GetExitCodeThread(hThread, out module) == 0)
                {
                    return LOADMODULERESULT.REMOTE_THREAD_DID_NOT_FINISH;
                }


                IntPtr MemoryFreeResult = VirtualFreeEx(process.Handle, hStringBuffer, 0, 0x8000 /* MEM_RELEASE */);
                if (MemoryFreeResult == IntPtr.Zero)
                {
                    return LOADMODULERESULT.MEMORY_NOT_DEALLOCATED;
                }

                return LOADMODULERESULT.SUCCESSFUL;
            }
            #endregion

        }
    }
}
