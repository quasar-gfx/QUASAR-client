rootProject.name = ("QuestClientApps")

val folderPath = "Apps/"

File(folderPath).listFiles()?.forEach { file ->
    if (file.isDirectory) {
        val dir = File("Apps/${file.name}/Projects/Android")
        include(":${file.name}")
        project(":${file.name}").projectDir = dir
    }
}
