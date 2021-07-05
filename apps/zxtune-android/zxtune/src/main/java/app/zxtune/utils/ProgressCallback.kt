package app.zxtune.utils

interface ProgressCallback {
    fun onProgressUpdate(done: Int, total: Int)
}
